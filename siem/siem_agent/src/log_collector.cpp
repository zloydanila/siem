#include "log_collector.h"
#include "utils.h"
#include <fstream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <limits.h>

using namespace std;

LogCollector::LogCollector(const AgentConfig& config, EventBuffer* buf) : cfg(config), buffer(buf){
}

static bool stat_file(const string& path, ino_t& ino, off_t& size){
    struct stat st{};
    if(stat(path.c_str(), &st) != 0) return false;
    ino = st.st_ino;
    size = st.st_size;
    return true;
}

static json make_event(const string& source_name, const string& line){
    json event;
    event["timestamp"] = get_timestamp();
    event["hostname"]  = get_hostname_str();
    event["source"]    = source_name;
    event["eventtype"] = "raw";
    event["severity"]  = "medium";
    event["user"]      = "";
    event["process"]   = "";
    event["command"]   = "";
    event["rawlog"]    = line;
    return event;
}

static bool extract_after(const string& s, const string& key, string& out){
    size_t p = s.find(key);
    if(p == string::npos) return false;
    p += key.size();
    if(p >= s.size()) return false;
    out = s.substr(p);
    return true;
}

static bool extract_between(const string& s, const string& left, const string& right, string& out){
    size_t p = s.find(left);
    if(p == string::npos) return false;
    p += left.size();
    size_t q = s.find(right, p);
    if(q == string::npos) return false;
    out = s.substr(p, q - p);
    return true;
}

static void parse_sys_auth(const string& source_name, const string& line, json& event){
    if(source_name != "syslog" && source_name != "auth.log") return;

    if(line.find(" sudo:") != string::npos){
        event["process"] = "sudo";
        event["eventtype"] = "sudo";
        event["severity"] = "medium";

        string u;
        if(extract_between(line, " sudo:", " :", u)){
            while(!u.empty() && u[0] == ' ') u.erase(u.begin());
            while(!u.empty() && u.back() == ' ') u.pop_back();
            event["user"] = u;
        }

        string cmd;
        if(extract_after(line, "COMMAND=", cmd)) event["command"] = cmd;
        return;
    }

    if(line.find("pam_unix(sudo:session):") != string::npos){
        event["process"] = "sudo";
        event["eventtype"] = "sudo";
        event["severity"] = "medium";

        string u;
        if(extract_between(line, "for user ", "(", u)) event["user"] = u;
        return;
    }

    if(line.find("sshd") != string::npos){
        event["process"] = "sshd";

        if(line.find("Accepted") != string::npos){
            event["eventtype"] = "userlogin";
            event["severity"] = "medium";
        } else if(line.find("Failed password") != string::npos){
            event["eventtype"] = "userlogin_failed";
            event["severity"] = "high";
        }

        string u;
        if(extract_between(line, "for ", " ", u)) event["user"] = u;
        return;
    }
}

void LogCollector::run_file(const string& source_name, const string& path){
    ifstream file(path);
    if(!file.is_open()){
        return;
    }

    ino_t ino0{};
    off_t size0{};
    stat_file(path, ino0, size0);

    file.seekg(0, ios::end);
    streampos last_pos = file.tellg();

    int fd = inotify_init1(IN_NONBLOCK);
    if(fd < 0){
        return;
    }

    uint32_t mask = IN_MODIFY | IN_MOVE_SELF | IN_DELETE_SELF | IN_ATTRIB | IN_CLOSE_WRITE;
    int wd = inotify_add_watch(fd, path.c_str(), mask);
    if(wd < 0){
        close(fd);
        return;
    }

    auto reopen = [&](){
        file.close();
        ifstream f2(path);
        if(!f2.is_open()) return false;
        file = std::move(f2);

        ino_t ino1{};
        off_t size1{};
        if(stat_file(path, ino1, size1)){
            ino0 = ino1;
        }

        file.seekg(0, ios::end);
        last_pos = file.tellg();
        return true;
    };

    auto read_new_lines = [&](){
        file.clear();
        file.seekg(last_pos);

        string line;
        while(getline(file, line)){
            json event = make_event(source_name, line);
            parse_sys_auth(source_name, line, event);
            buffer->push(event);
        }

        file.clear();
        file.seekg(0, ios::end);
        last_pos = file.tellg();
    };

    while(true){
        ino_t ino1{};
        off_t size1{};
        if(stat_file(path, ino1, size1)){
            if(ino1 != ino0){
                reopen();
            } else if(size1 < (off_t)last_pos){
                file.clear();
                file.seekg(0, ios::beg);
                last_pos = file.tellg();
            }
        }

        char buf[4096];
        ssize_t len = read(fd, buf, sizeof(buf));
        if(len > 0){
            ssize_t i = 0;
            bool need_reopen = false;
            bool need_read = false;

            while(i < len){
                inotify_event* ev = (inotify_event*)(buf + i);

                if(ev->mask & (IN_MOVE_SELF | IN_DELETE_SELF)){
                    need_reopen = true;
                }
                if(ev->mask & (IN_MODIFY | IN_CLOSE_WRITE)){
                    need_read = true;
                }
                if(ev->mask & IN_ATTRIB){
                    need_read = true;
                }

                i += sizeof(inotify_event) + ev->len;
            }

            if(need_reopen){
                for(int t = 0; t < 10; t++){
                    if(reopen()) break;
                    this_thread::sleep_for(chrono::milliseconds(200));
                }
            }

            if(need_read){
                read_new_lines();
            }
        } else {
            this_thread::sleep_for(chrono::milliseconds(200));
        }
    }

    inotify_rm_watch(fd, wd);
    close(fd);
}

string LogCollector::source_to_path(const string& src){
    if(src == "syslog") return "/var/log/syslog";
    if(src == "auth.log") return "/var/log/auth.log";
    if(src == "auditd") return "/var/log/audit/audit.log";
    if(src == "bashhistory") return string(getenv("HOME") ? getenv("HOME") : "") + "/.bash_history";
    return "";
}

string LogCollector::guess_source_name(const string& src){
    return src;
}

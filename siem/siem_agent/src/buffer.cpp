#include "buffer.h"
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
using std::lock_guard;
using json = nlohmann::json;

static json normalize_event(json e){
    auto ensure_str = [&](const char* k){
        if(!e.contains(k) || !e[k].is_string()) e[k] = "";
    };

    ensure_str("timestamp");
    ensure_str("hostname");
    ensure_str("source");
    ensure_str("eventtype");
    ensure_str("severity");
    ensure_str("user");
    ensure_str("process");
    ensure_str("command");
    ensure_str("rawlog");

    return e;
}

static bool drop_event(const json& e){
    if(e.contains("source") && e["source"].is_string() &&
       e.contains("eventtype") && e["eventtype"].is_string()){
        string src = e["source"].get<string>();
        string type = e["eventtype"].get<string>();
        if(src == "syslog" && type == "raw") return true;
    }
    return false;
}

static string parent_dir(const string& p){
    size_t pos = p.find_last_of('/');
    if(pos == string::npos) return "";
    return p.substr(0, pos);
}

EventBuffer::EventBuffer(unsigned int cap, const string& spool)
    : head(0), count(0), capacity(cap), spool_path(spool){
}


void EventBuffer::ensure_dir_nolock(const string& dir){
    if(dir.empty()) return;

    string cur;
    for(size_t i = 0; i < dir.size(); ++i){
        char c = dir[i];
        cur.push_back(c);

        if(c == '/' || i + 1 == dir.size()){
            if(cur.size() == 1 && cur[0] == '/') continue;
            if(mkdir(cur.c_str(), 0755) != 0 && errno != EEXIST){
                return;
            }
        }
    }
}

void EventBuffer::spool_append_nolock(const json& e){
    string dir = parent_dir(spool_path);
    ensure_dir_nolock(dir);

    std::ofstream f(spool_path, std::ios::app);
    if(!f.is_open()){
        return;
    }

    f << normalize_event(e).dump() << "\n";
    f.flush();
}

bool EventBuffer::spool_pop_one_nolock(json& out){
    std::ifstream in(spool_path);
    if(!in.is_open()) return false;

    string first;
    if(!std::getline(in, first)) return false;

    string rest_path = spool_path + ".tmp";
    std::ofstream outrest(rest_path, std::ios::trunc);
    if(!outrest.is_open()) return false;

    string line;
    while(std::getline(in, line)){
        outrest << line << "\n";
    }

    in.close();
    outrest.close();

    rename(rest_path.c_str(), spool_path.c_str());

    try{
        out = json::parse(first);
    } catch(...) {
        return false;
    }
    return true;
}

void EventBuffer::push(const json& e){
    json ev = normalize_event(e);
    if(drop_event(ev)) return;

    lock_guard<mutex> lock(m);

    if(events.get_size() < capacity){
        while(events.get_size() < capacity) events.push_back(json::object());
    }

    if(count < capacity){
        unsigned int idx = (head + count) % capacity;
        events[idx] = ev;
        count++;
        return;
    }

    spool_append_nolock(ev);
}

Vector<json> EventBuffer::pop_batch(int max_count){
    lock_guard<mutex> lock(m);
    Vector<json> batch;
    if(max_count <= 0) return batch;

    int take = 0;

    while(take < max_count){
        json e;
        if(spool_pop_one_nolock(e)){
            batch.push_back(normalize_event(e));
            take++;
            continue;
        }
        break;
    }

    while(take < max_count && count > 0){
        json e = events[head];
        head = (head + 1) % capacity;
        count--;
        batch.push_back(normalize_event(e));
        take++;
    }

    return batch;
}

void EventBuffer::push_front_spool(const Vector<json>& batch){
    lock_guard<mutex> lock(m);
    if(batch.get_size() == 0) return;

    std::ifstream in(spool_path);
    string old;
    if(in.is_open()){
        old.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();
    }

    string dir = parent_dir(spool_path);
    ensure_dir_nolock(dir);

    std::ofstream out(spool_path, std::ios::trunc);
    if(!out.is_open()) return;

    for(unsigned int i = 0; i < batch.get_size(); i++){
        out << normalize_event(batch[i]).dump() << "\n";
    }
    if(!old.empty()){
        out << old;
        if(old.back() != '\n') out << "\n";
    }
}

unsigned int EventBuffer::size() const{
    lock_guard<mutex> lock(m);
    return count;
}

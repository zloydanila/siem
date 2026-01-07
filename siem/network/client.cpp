#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <cctype>

#include "../include/json.hpp"

using json = nlohmann::json;
using namespace std;

static bool send_all(int fd, const string& s){
    size_t total = 0;
    while(total < s.size()){
        ssize_t sent = send(fd, s.data() + total, s.size() - total, 0);
        if(sent <= 0) return false;
        total += (size_t)sent;
    }
    return true;
}

static bool read_line(int fd, string& out){
    out.clear();
    char ch;
    while(true){
        ssize_t n = recv(fd, &ch, 1, 0);
        if(n == 0) return false;
        if(n < 0) return false;
        if(ch == '\n') break;
        out.push_back(ch);
        if(out.size() > 2000000) return false;
    }
    return true;
}

static string trim(string s){
    size_t l = 0;
    while(l < s.size() && isspace((unsigned char)s[l])) l++;
    size_t r = s.size();
    while(r > l && isspace((unsigned char)s[r - 1])) r--;
    return s.substr(l, r - l);
}

static bool next_token(const string& s, size_t& pos, string& tok){
    while(pos < s.size() && isspace((unsigned char)s[pos])) pos++;
    if(pos >= s.size()) return false;

    size_t start = pos;
    while(pos < s.size() && !isspace((unsigned char)s[pos])) pos++;
    tok = s.substr(start, pos - start);
    return true;
}

static bool parse_quoted(const string& s, size_t& i, string& out){
    if(i >= s.size() || s[i] != '"') return false;
    i++;
    string res;
    while(i < s.size()){
        char c = s[i++];
        if(c == '"'){
            out = res;
            return true;
        }
        if(c == '\\' && i < s.size()){
            char n = s[i++];
            if(n == 'n') res.push_back('\n');
            else if(n == 't') res.push_back('\t');
            else if(n == 'r') res.push_back('\r');
            else res.push_back(n);
            continue;
        }
        res.push_back(c);
    }
    return false;
}

static json parse_scalar(const string& vraw){
    string v = trim(vraw);
    if(v == "true") return true;
    if(v == "false") return false;
    if(v == "null") return nullptr;

    bool is_int = !v.empty();
    size_t j = 0;
    if(v.size() > 0 && (v[0] == '-' || v[0] == '+')) j = 1;
    for(; j < v.size(); j++){
        if(!isdigit((unsigned char)v[j])) { is_int = false; break; }
    }
    if(is_int){
        try { return stoi(v); } catch(...) {}
    }

    bool is_num = !v.empty();
    bool dot = false;
    j = 0;
    if(v.size() > 0 && (v[0] == '-' || v[0] == '+')) j = 1;
    for(; j < v.size(); j++){
        if(v[j] == '.'){
            if(dot){ is_num = false; break; }
            dot = true;
            continue;
        }
        if(!isdigit((unsigned char)v[j])) { is_num = false; break; }
    }
    if(is_num && dot){
        try { return stod(v); } catch(...) {}
    }

    return v;
}

static json parse_kv_eq_list(const string& raw){
    string s = trim(raw);
    json obj = json::object();
    if(s.empty()) return obj;

    size_t i = 0;
    while(i < s.size()){
        while(i < s.size() && (isspace((unsigned char)s[i]) || s[i] == ',')) i++;
        if(i >= s.size()) break;

        size_t k0 = i;
        while(i < s.size() && s[i] != '=' && s[i] != ',' ) i++;
        if(i >= s.size() || s[i] != '=') break;

        string key = trim(s.substr(k0, i - k0));
        i++;

        while(i < s.size() && isspace((unsigned char)s[i])) i++;

        string val;
        if(i < s.size() && s[i] == '"'){
            if(!parse_quoted(s, i, val)) break;
        }else{
            size_t v0 = i;
            while(i < s.size() && s[i] != ',') i++;
            val = trim(s.substr(v0, i - v0));
        }

        if(!key.empty()){
            obj[key] = parse_scalar(val);
        }

        while(i < s.size() && s[i] != ',') i++;
        if(i < s.size() && s[i] == ',') i++;
    }

    return obj;
}

static json parse_simple_where(const string& raw){
    string s = trim(raw);
    if(s.empty()) return json::object();

    size_t pos = 0;
    string field, op;
    if(!next_token(s, pos, field)) return json::object();
    if(!next_token(s, pos, op)) return json::object();
    string value = trim(s.substr(pos));

    json v = parse_scalar(value);

    if(op == "eq") return json::object({{field, v}});
    if(op == "ne") return json::object({{field, json::object({{"$ne", v}})}});
    if(op == "gt") return json::object({{field, json::object({{"$gt", v}})}});
    if(op == "lt") return json::object({{field, json::object({{"$lt", v}})}});
    if(op == "gte") return json::object({{field, json::object({{"$gte", v}})}});
    if(op == "lte") return json::object({{field, json::object({{"$lte", v}})}});

    return json::object({{field, v}});
}

static void usage(){
    cout << "Usage:\n";
    cout << "  dbclient --host <host> --port <port> --database <name>\n";
    cout << "Commands:\n";
    cout << "  INSERT <collection> key=value,key=value,...\n";
    cout << "  FIND <collection> <field> <op> <value>\n";
    cout << "  DELETE <collection> <field> <op> <value>\n";
    cout << "  exit\n";
}

int main(int argc, char** argv){
    string host = "127.0.0.1";
    int port = 8080;
    string database = "mydatabase";

    for(int i = 1; i < argc; i++){
        string a = argv[i];
        if(a == "--host" && i + 1 < argc) host = argv[++i];
        else if(a == "--port" && i + 1 < argc) port = stoi(argv[++i]);
        else if(a == "--database" && i + 1 < argc) database = argv[++i];
        else if(a == "--help" || a == "-h"){ usage(); return 0; }
        else{
            cout << "Unknown argument: " << a << "\n";
            usage();
            return 1;
        }
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0){ perror("socket"); return 1; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if(inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0){
        cerr << "Некорректный адрес: " << host << endl;
        return 1;
    }

    if(connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0){
        perror("connect");
        return 1;
    }

    cout << "Подключено к " << host << ":" << port << " (database=" << database << ")\n";
    cout << "REPL: INSERT/FIND/DELETE или exit\n";

    for(;;){
        cout << "> ";
        string line;
        if(!getline(cin, line)) break;
        line = trim(line);
        if(line == "exit") break;
        if(line.empty()) continue;

        size_t pos = 0;
        string cmd, collection;
        if(!next_token(line, pos, cmd) || !next_token(line, pos, collection)){
            cout << "Ошибка: формат команды\n";
            continue;
        }

        json req;
        if(cmd == "INSERT"){
            json data = parse_kv_eq_list(trim(line.substr(pos)));
            req = {{"database", database},
                   {"collection", collection},
                   {"operation", "insert"},
                   {"data", data}};
        }else if(cmd == "FIND"){
            json q = parse_simple_where(trim(line.substr(pos)));
            req = {{"database", database},
                   {"collection", collection},
                   {"operation", "find"},
                   {"query", q}};
        }else if(cmd == "DELETE"){
            json q = parse_simple_where(trim(line.substr(pos)));
            req = {{"database", database},
                   {"collection", collection},
                   {"operation", "delete"},
                   {"query", q}};
        }else{
            cout << "Неизвестная команда\n";
            continue;
        }

        string out = req.dump() + "\n";
        if(!send_all(fd, out)){
            cerr << "Ошибка отправки\n";
            break;
        }

        string resp_line;
        if(!read_line(fd, resp_line)){
            cerr << "Сервер отключился\n";
            break;
        }

        try{
            json resp = json::parse(resp_line);
            cout << resp.dump(2) << "\n";
        }catch(...){
            cout << resp_line << "\n";
        }
    }

    close(fd);
    return 0;
}

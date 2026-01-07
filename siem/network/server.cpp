#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cctype>
#include <functional>

#include "../include/json.hpp"
#include "../database/database.h"
#include "../collection/collection.h"
#include "../containers/queue.h"
#include "../containers/hash_map.h"
#include "../containers/vector.h"

using namespace std;
using json = nlohmann::json;

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

static json ok(const string& msg, const json& data, int count){
    return {{"status","success"},{"message",msg},{"data",data},{"count",count}};
}

static json err(const string& msg){
    return {{"status","error"},{"message",msg},{"data",json::array()},{"count",0}};
}

static string trim(string s){
    size_t l = 0;
    while(l < s.size() && isspace((unsigned char)s[l])) l++;
    size_t r = s.size();
    while(r > l && isspace((unsigned char)s[r-1])) r--;
    return s.substr(l, r-l);
}

static HashMap<string, Database*> g_dbs;
static Vector<Database*> g_db_ptrs;
static mutex g_dbs_mutex;
static mutex g_io_mutex;

static int g_port = 8080;
static string g_schema_path = "schema.json";
static string g_data_root = "data";

static void usage(){
    cout << "использование: db_server [--port 8080] [--schema путь_к_schema.json] [--data-root папка_данных]\n";
}

static void parse_args(int argc, char** argv){
    for(int i = 1; i < argc; i++){
        string a = argv[i];
        if(a == "--port" && i + 1 < argc) g_port = stoi(argv[++i]);
        else if(a == "--schema" && i + 1 < argc) g_schema_path = argv[++i];
        else if(a == "--data-root" && i + 1 < argc) g_data_root = argv[++i];
        else if(a == "--help" || a == "-h"){ usage(); exit(0); }
        else{
            cerr << "неизвестный аргумент: " << a << "\n";
            usage();
            exit(1);
        }
    }

    g_schema_path = trim(g_schema_path);
    g_data_root = trim(g_data_root);

    if(g_schema_path.empty()) throw runtime_error("пустой путь к schema.json");
    if(g_data_root.empty()) throw runtime_error("пустая папка data-root");
}

static Database& get_db_by_name(const string& dbname){
    if(dbname.empty()) throw runtime_error("пустое имя базы данных");

    lock_guard<mutex> lock(g_dbs_mutex);

    if(g_dbs.contains(dbname)){
        return *g_dbs[dbname];
    }

    Database* db = new Database(g_schema_path, dbname, g_data_root);
    g_dbs.insert(dbname, db);
    g_db_ptrs.push_back(db);
    return *db;
}

static json process_request(const string& line){
    json req;
    try{
        req = json::parse(line);
    }catch(const exception& e){
        return err(string("некорректный запрос: ") + e.what());
    }

    if(!req.contains("database") || !req["database"].is_string()){
        return err("поле database обязательно");
    }
    if(!req.contains("collection") || !req["collection"].is_string()){
        return err("поле collection обязательно");
    }
    if(!req.contains("operation") || !req["operation"].is_string()){
        return err("поле operation обязательно");
    }

    string dbname = req["database"].get<string>();
    string collection = req["collection"].get<string>();
    string operation = req["operation"].get<string>();

    if(dbname.empty()) return err("поле database пустое");
    if(collection.empty()) return err("поле collection пустое");
    if(operation.empty()) return err("поле operation пустое");

    json query = req.value("query", json::object());

    lock_guard<mutex> io_lock(g_io_mutex);

    Database* dbp = nullptr;
    Collection* collp = nullptr;
    try{
        dbp = &get_db_by_name(dbname);
        collp = &dbp->get_collection(collection);
    }catch(const exception& e){
        return err(e.what());
    }

    Collection& coll = *collp;

    if(operation == "find"){
        auto docs = coll.find(query, json::object(), json::object(), 0);

        json data = json::array();
        for(unsigned int i = 0; i < docs.get_size(); i++){
            data.push_back(docs[i]);
        }
        return ok("документы получены", data, (int)docs.get_size());
    }

    if(operation == "insert"){
        if(!req.contains("data") || !req["data"].is_object()){
            return err("для insert поле data должно быть объектом");
        }

        json doc = req["data"];
        if(!doc.contains("_id")){
            static int gen2 = 1;
            doc["_id"] = string("auto_") + to_string(gen2++);
        }

        if(!doc["_id"].is_string()){
            return err("поле _id должно быть строкой");
        }

        string id = doc["_id"].get<string>();

        try{
            coll.insert(doc);
        }catch(const exception& e){
            return err(string("ошибка вставки: ") + e.what());
        }

        json chk = coll.find_one({{"_id", id}}, json::object(), json::object());
        if(chk.empty()){
            return err("insert выполнен, но документ не найден после сохранения");
        }

        return ok("документ добавлен", json::array(), 1);
    }

    if(operation == "delete"){
        int deleted = coll.delete_many(query);
        return ok("удаление выполнено", json::array(), deleted);
    }

    return err("неизвестная операция");
}

static string parse_http_request(const string& request_line, string& body) {

    size_t first_space = request_line.find(' ');
    size_t second_space = request_line.rfind(' ');
    
    if(first_space == string::npos || second_space == string::npos) {
        return "";
    }
    
    string method = request_line.substr(0, first_space);
    string path = request_line.substr(first_space + 1, second_space - first_space - 1);
    
    return path;
}

static bool send_http_response(int fd, const json& response, int status_code = 200) {
    string json_body = response.dump();
    
    string http_response = "HTTP/1.1 " + to_string(status_code) + " OK\r\n";
    http_response += "Content-Type: application/json\r\n";
    http_response += "Content-Length: " + to_string(json_body.size()) + "\r\n";
    http_response += "Connection: close\r\n";
    http_response += "Access-Control-Allow-Origin: *\r\n";
    http_response += "\r\n";
    http_response += json_body;
    
    return send_all(fd, http_response);
}

static void handle_client(int client_fd){
    for(;;){
        string line;
        if(!read_line(client_fd, line)) break;
        if(line.empty()) continue;


        if(line.find("GET") == 0 || line.find("POST") == 0) {

            while(true) {
                string header;
                if(!read_line(client_fd, header)) break;
                if(header.empty()) break; 
            }
            

            if(line.find("GET /api/events") == 0) {
                try {
                    Database& db = get_db_by_name("siem");
                    Collection& coll = db.get_collection("events");
                    auto docs = coll.find(json::object(), json::object(), json::object(), 0);
                    
                    json data = json::array();
                    for(unsigned int i = 0; i < docs.get_size(); i++){
                        data.push_back(docs[i]);
                    }
                    
                    json resp = ok("события получены", data, (int)docs.get_size());
                    send_http_response(client_fd, resp);
                } catch(const exception& e) {
                    json resp = err(e.what());
                    send_http_response(client_fd, resp, 500);
                }
            }
            
            else if(line.find("POST /api/events") == 0) {
                // Читаем тело запроса
                string body_line;
                if(read_line(client_fd, body_line)) {
                    json resp = process_request(body_line);
                    send_http_response(client_fd, resp);
                }
            }
           
            else {
                string body_line;
                if(read_line(client_fd, body_line)) {
                    json resp = process_request(body_line);
                    send_http_response(client_fd, resp);
                }
            }
        }
        else {
          
            json resp = process_request(line);
            string reply = resp.dump() + "\n";
            if(!send_all(client_fd, reply)) break;
        }
    }
    close(client_fd);
}

int main(int argc, char** argv){
    try{
        parse_args(argc, argv);
    }catch(const exception& e){
        cerr << "ошибка аргументов: " << e.what() << "\n";
        return 1;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0){
        perror("socket");
        return 1;
    }

    int opt = 1;
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
        perror("setsockopt");
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(g_port);

    if(bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0){
        perror("bind");
        return 1;
    }

    if(listen(server_fd, 16) < 0){
        perror("listen");
        return 1;
    }

    Queue<int> q;
    mutex q_m;
    condition_variable q_cv;

    const int WORKERS = 5;
    thread* workers = new thread[WORKERS];

    for(int i = 0; i < WORKERS; i++){
        workers[i] = thread([&](){
            while(true){
                int fd;
                {
                    unique_lock<mutex> lock(q_m);
                    q_cv.wait(lock, [&](){ return !q.empty(); });
                    if(!q.pop(fd)) continue;
                }
                handle_client(fd);
            }
        });
    }

    while(true){
        int client_fd = accept(server_fd, nullptr, nullptr);
        if(client_fd < 0){
            perror("accept");
            continue;
        }

        {
            lock_guard<mutex> lock(q_m);
            q.push(client_fd);
        }
        q_cv.notify_one();
    }

    return 0;
}

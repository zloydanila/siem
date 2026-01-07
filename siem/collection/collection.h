#pragma once
#include <string>
#include <fstream>
#include <iostream>
#include "../containers/vector.h"
#include "../containers/hash_map.h"
#include "../include/json.hpp"

using namespace std;
using json = nlohmann::json;

class Collection {
private:
    string name;
    string db_path;
    int tuples_limit;
    json structure;

    string get_file_path(int file_num) const;
    bool file_exists(const string& path) const;
    void create_directory(const string& path) const;
    int get_last_file_number() const;
    int count_documents_in_file(const string& file_path) const;

public:
    Collection() : name(""), db_path(""), tuples_limit(0), structure(json::object()) {}
    Collection(const string& name, const string& db_path,
               int tuples_limit, const json& structure);

    void insert(const json& document);
    void insert_many(const Vector<json>& documents);

    Vector<json> find(const json& filter = json::object(),
                      const json& projection = json::object(),
                      const json& sort = json::object(),
                      int limit = 0) const;

    json find_one(const json& filter, const json& projection, const json& sort) const;

    int update_one(const json& filter, const json& update_data);
    int update_many(const json& filter, const json& update_data);
    int delete_one(const json& filter);
    int delete_many(const json& filter);

    string get_name() const { return name; }

    Vector<string> get_file_info() const {
        Vector<string> files;
        int file_num = 1;
        while(file_exists(get_file_path(file_num))){
            files.push_back(get_file_path(file_num));
            file_num++;
        }
        return files;
    }

    void print_file_contents() {
        int file_num = 1;
        while(file_exists(get_file_path(file_num))){
            cout << "Файл " << get_file_path(file_num) << ":" << endl;
            try {
                ifstream file(get_file_path(file_num));
                json data;
                file >> data;
                cout << data.dump(2, ' ', false, json::error_handler_t::replace) << endl;
                file.close();
            } catch(const exception& e) {
                cout << "Ошибка чтения файла: " << e.what() << endl;
            }
            file_num++;
        }
    }
};

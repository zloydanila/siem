#include "collection.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include "../include/json.hpp"

using namespace std;
namespace fs = filesystem;
using json = nlohmann::json;

static bool check_operators(const json& document, const string& field, const json& operators) {
    if(!document.contains(field)){
        return false;
    }

    for (auto it = operators.begin(); it != operators.end(); ++it) {
        string op = it.key();
        auto& op_value = it.value();

        if(op == "$eq"){
            if(document[field] != op_value) return false;
        }else if(op == "$ne"){
            if(document[field] == op_value) return false;
        }else if(op == "$gt"){
            if(!(document[field] > op_value)) return false;
        }else if(op == "$lt"){
            if(!(document[field] < op_value)) return false;
        }else if(op == "$gte"){
            if(!(document[field] >= op_value)) return false;
        }else if(op == "$lte"){
            if(!(document[field] <= op_value)) return false;
        }else if(op == "$in"){
            if(!op_value.is_array()) return false;
            bool found = false;
            for(const auto& item : op_value){
                if(document[field] == item){
                    found = true;
                    break;
                }
            }
            if(!found) return false;
        }else if(op == "$nin"){
            if(!op_value.is_array()) return false;
            for(const auto& item : op_value){
                if(document[field] == item){
                    return false;
                }
            }
        }
    }
    return true;
}

static bool matches_filter(const json& document, const json& filter) {
    if (filter.empty()) return true;
    if (!filter.is_object()) return false;

    for (auto it = filter.begin(); it != filter.end(); ++it) {
        string key = it.key();
        auto& condition = it.value();

        if (key == "$and") {
            if (!condition.is_array()) return false;
            for (const auto& cond : condition) {
                if (!matches_filter(document, cond)) return false;
            }
        }
        else if (key == "$or") {
            if (!condition.is_array()) return false;
            bool found_match = false;
            for (const auto& cond : condition) {
                if (matches_filter(document, cond)) {
                    found_match = true;
                    break;
                }
            }
            if (!found_match) return false;
        }
        else if (key == "$not") {
            if (matches_filter(document, condition)) return false;
        }
        else if (condition.is_primitive() || condition.is_string()) {
            if (!document.contains(key) || document[key] != condition) {
                return false;
            }
        }
        else if (condition.is_object()) {
            if (!check_operators(document, key, condition)) {
                return false;
            }
        }
    }
    return true;
}

static bool compare_documents(const json& a, const json& b, const json& sort_rules) {
    for (auto& [field, direction] : sort_rules.items()) {
        if (!a.contains(field) && !b.contains(field)) continue;
        if (!a.contains(field)) return false;
        if (!b.contains(field)) return true;

        if (a[field] < b[field]) return direction == 1;
        if (a[field] > b[field]) return direction == -1;
    }
    return false;
}

static void apply_update_operators(json& document, const json& update_data) {
    for (auto& [operator_name, operations] : update_data.items()) {
        if (operator_name == "$set") {
            for (auto& [field, value] : operations.items()) {
                document[field] = value;
            }
        }
        else if (operator_name == "$inc") {
            for (auto& [field, increment] : operations.items()) {
                if (!increment.is_number()) continue;

                if (!document.contains(field)) {
                    document[field] = increment;
                    continue;
                }

                if (document[field].is_number_integer() && increment.is_number_integer()) {
                    long long a = document[field].get<long long>();
                    long long b = increment.get<long long>();
                    document[field] = a + b;
                } else if (document[field].is_number() && increment.is_number()) {
                    double a = document[field].get<double>();
                    double b = increment.get<double>();
                    document[field] = a + b;
                }
            }
        }
        else if (operator_name == "$push") {
            for (auto& [field, new_value] : operations.items()) {
                if (!document.contains(field)) {
                    document[field] = json::array();
                }
                if (document[field].is_array()) {
                    document[field].push_back(new_value);
                }
            }
        }
    }
}

Collection::Collection(const string& name, const string& db_path,
                       int tuples_limit, const json& structure)
    : name(name), db_path(db_path), tuples_limit(tuples_limit), structure(structure){

    string collection_path = db_path + name + "/";
    create_directory(collection_path);

    string first_file = get_file_path(1);
    if(!file_exists(first_file)){
        ofstream file(first_file);
        file << "[]";
        file.close();
    }
}

bool Collection::file_exists(const string& path) const{
    return fs::exists(path);
}

void Collection::create_directory(const string& path) const{
    if(path.empty()){
        throw runtime_error("Пустой путь для создания директории");
    }
    fs::create_directories(path);
}

string Collection::get_file_path(int file_num) const{
    return db_path + name + "/" + to_string(file_num) + ".json";
}

int Collection::get_last_file_number() const{
    int file_num = 1;
    while(file_exists(get_file_path(file_num))){
        file_num++;
    }
    return file_num - 1;
}

int Collection::count_documents_in_file(const string& file_path) const {
    if(!file_exists(file_path)) return 0;

    try{
        ifstream file(file_path);
        json data;
        file >> data;
        if(!data.is_array()) return 0;
        return (int)data.size();
    } catch(const exception&){
        return 0;
    }
}

void Collection::insert(const json& document) {
    if (!document.contains("_id")) {
        throw runtime_error("Документ должен содержать поле _id");
    }
    if(!document["_id"].is_string()){
        throw runtime_error("Поле _id должно быть строкой");
    }

    string document_id = document["_id"].get<string>();

    auto existing = find_one({{"_id", document_id}}, json::object(), json::object());
    if (!existing.empty()) {
        throw runtime_error("Документ с _id уже существует");
    }

    int last_file_num = get_last_file_number();
    int target_file_num = last_file_num <= 0 ? 1 : last_file_num;
    string target_path = get_file_path(target_file_num);

    int current_count = count_documents_in_file(target_path);
    if(current_count >= tuples_limit){
        target_file_num++;
        target_path = get_file_path(target_file_num);
        ofstream new_file(target_path);
        new_file << "[]";
        new_file.close();
    } else if(!file_exists(target_path)) {
        ofstream new_file(target_path);
        new_file << "[]";
        new_file.close();
    }

    json data = json::array();

    {
        ifstream in(target_path);
        if(in.is_open()){
            try{
                in >> data;
            }catch(...){
                data = json::array();
            }
        }
    }

    if(!data.is_array()){
        data = json::array();
    }

    data.push_back(document);

    {
        ofstream out(target_path, ios::trunc);
        if(!out.is_open()){
            throw runtime_error("Не удалось открыть файл коллекции для записи");
        }
        out << data.dump(4, ' ', false, json::error_handler_t::replace);
        out.close();
    }
}

void Collection::insert_many(const Vector<json>& documents) {
    Vector<string> ids;

    for (unsigned int i = 0; i < documents.get_size(); i++) {
        const auto& doc = documents[i];
        if (!doc.contains("_id") || !doc["_id"].is_string()) {
            throw runtime_error("Все документы должны содержать строковый _id");
        }
        string doc_id = doc["_id"].get<string>();

        bool found_duplicate = false;
        for (unsigned int j = 0; j < ids.get_size(); j++) {
            if (ids[j] == doc_id) {
                found_duplicate = true;
                break;
            }
        }
        if (found_duplicate) {
            throw runtime_error("Найден дубликат _id в переданных документах");
        }
        ids.push_back(doc_id);

        auto existing = find_one({{"_id", doc_id}}, json::object(), json::object());
        if (!existing.empty()) {
            throw runtime_error("Документ с _id уже существует в базе");
        }
    }

    for (unsigned int i = 0; i < documents.get_size(); i++) {
        insert(documents[i]);
    }
}

int Collection::update_many(const json& filter, const json& update_data){
    int file_num = 1;
    int updated_count = 0;

    while(true){
        string file_path = get_file_path(file_num);
        if(!file_exists(file_path)) break;

        try{
            ifstream file_in(file_path);
            json data;
            file_in >> data;
            file_in.close();

            if(!data.is_array()){
                file_num++;
                continue;
            }

            bool changes_made = false;
            for(auto& document : data){
                if(matches_filter(document, filter)){
                    apply_update_operators(document, update_data);
                    changes_made = true;
                    updated_count++;
                }
            }
            if(changes_made){
                ofstream file_out(file_path, ios::trunc);
                file_out << data.dump(4, ' ', false, json::error_handler_t::replace);
                file_out.close();
            }
        }catch(const exception&){
        }
        file_num++;
    }

    return updated_count;
}

int Collection::update_one(const json& filter, const json& update_data){
    int file_num = 1;

    while(true){
        string file_path = get_file_path(file_num);
        if(!file_exists(file_path)) break;

        try{
            ifstream file_in(file_path);
            json data;
            file_in >> data;
            file_in.close();

            if(!data.is_array()){
                file_num++;
                continue;
            }

            for(auto& document : data){
                if(matches_filter(document, filter)){
                    apply_update_operators(document, update_data);

                    ofstream file_out(file_path, ios::trunc);
                    file_out << data.dump(4, ' ', false, json::error_handler_t::replace);
                    file_out.close();
                    return 1;
                }
            }
        }catch(const exception&){
        }
        file_num++;
    }
    return 0;
}

int Collection::delete_many(const json& filter){
    int file_num = 1;
    int deleted_count = 0;

    while(true){
        string file_path = get_file_path(file_num);
        if(!file_exists(file_path)) break;

        try{
            ifstream file_in(file_path);
            json data;
            file_in >> data;
            file_in.close();

            if(!data.is_array()){
                file_num++;
                continue;
            }

            json new_data = json::array();
            bool changes_made = false;
            for(auto& document : data){
                if(matches_filter(document, filter)){
                    changes_made = true;
                    deleted_count++;
                }else{
                    new_data.push_back(document);
                }
            }
            if(changes_made){
                ofstream file_out(file_path, ios::trunc);
                file_out << new_data.dump(4, ' ', false, json::error_handler_t::replace);
                file_out.close();
            }
        }catch(const exception&){
        }
        file_num++;
    }

    return deleted_count;
}

int Collection::delete_one(const json& filter){
    int file_num = 1;

    while(true){
        string file_path = get_file_path(file_num);
        if(!file_exists(file_path)) break;

        try{
            ifstream file_in(file_path);
            json data;
            file_in >> data;
            file_in.close();

            if(!data.is_array()){
                file_num++;
                continue;
            }

            json new_data = json::array();
            bool deleted = false;
            for(auto& document : data){
                if(!deleted && matches_filter(document, filter)){
                    deleted = true;
                }else{
                    new_data.push_back(document);
                }
            }
            if(deleted){
                ofstream file_out(file_path, ios::trunc);
                file_out << new_data.dump(4, ' ', false, json::error_handler_t::replace);
                file_out.close();
                return 1;
            }
        }catch(const exception&){
        }
        file_num++;
    }

    return 0;
}

Vector<json> Collection::find(const json& filter, const json& projection, const json& sort, int limit) const{
    Vector<json> results;
    int file_num = 1;

    while (true) {
        string file_path = get_file_path(file_num);
        if (!file_exists(file_path)) break;

        try {
            ifstream file(file_path);
            if (!file.is_open()) { file_num++; continue; }

            json data;
            file >> data;

            if (!data.is_array()) { file_num++; continue; }

            for (const auto& document : data) {
                if (matches_filter(document, filter)) {
                    json projected_doc;
                    if (projection.empty()) {
                        projected_doc = document;
                    } else {
                        for (auto it = projection.begin(); it != projection.end(); ++it) {
                            string field_name = it.value();
                            if (document.contains(field_name)) {
                                projected_doc[field_name] = document[field_name];
                            }
                        }
                    }
                    results.push_back(projected_doc);

                    if (limit > 0 && results.get_size() >= (unsigned int)limit) {
                        return results;
                    }
                }
            }

        } catch (const exception&) {
        }

        file_num++;
    }

    if (!sort.empty() && results.get_size() > 0) {
        for (unsigned int i = 0; i < results.get_size() - 1; i++) {
            for (unsigned int j = 0; j < results.get_size() - i - 1; j++) {
                if (!compare_documents(results[j], results[j + 1], sort)) {
                    json temp = results[j];
                    results[j] = results[j + 1];
                    results[j + 1] = temp;
                }
            }
        }
    }

    return results;
}

json Collection::find_one(const json& filter, const json& projection, const json& sort) const {
    Vector<json> results = find(filter, projection, sort, 1);
    if (results.get_size() > 0) {
        return results[0];
    }
    return json();
}

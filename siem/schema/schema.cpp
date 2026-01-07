#include "schema.h"
#include <fstream>
#include <stdexcept>
#include <string>

using namespace std;
using json = nlohmann::json;

static string j_get_string(const json& j, const string& k){
    if(!j.contains(k) || !j[k].is_string()){
        throw runtime_error("schema.json: поле " + k + " должно быть строкой");
    }
    return j[k].get<string>();
}

static int j_get_int(const json& j, const string& k){
    if(!j.contains(k) || !j[k].is_number_integer()){
        throw runtime_error("schema.json: поле " + k + " должно быть целым числом");
    }
    return j[k].get<int>();
}

Schema load_schema(const string& filename){
    ifstream file(filename);
    if(!file.is_open()){
        throw runtime_error("не получилось открыть файл: " + filename);
    }

    json j;
    file >> j;

    if(!j.is_object()){
        throw runtime_error("schema.json: корень должен быть объектом");
    }

    Schema schema;
    schema.name = j_get_string(j, "name");
    schema.tuples_limit = j_get_int(j, "tuples_limit");

    if(!j.contains("structure") || !j["structure"].is_object()){
        throw runtime_error("schema.json: поле structure должно быть объектом");
    }

    auto structure_obj = j["structure"];
    for(auto it = structure_obj.begin(); it != structure_obj.end(); ++it){
        schema.structure.insert(it.key(), it.value());
    }

    return schema;
}

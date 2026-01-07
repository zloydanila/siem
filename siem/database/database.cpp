#include "database.h"
#include <filesystem>
#include <stdexcept>

using namespace std;
namespace fs = filesystem;

Database::Database(const string& schema_file, const string& db_name, const string& data_root){
    if(schema_file.empty()) throw runtime_error("Пустой путь schema.json");
    if(db_name.empty()) throw runtime_error("Пустое имя базы данных");
    if(data_root.empty()) throw runtime_error("Пустая папка data-root");

    schema = load_schema(schema_file);

    base_path = data_root + "/" + db_name + "/";
    fs::create_directories(base_path);

    auto it = schema.structure.begin();
    auto end_it = schema.structure.end();
    while(it != end_it){
        auto kv = *it;
        string collection_name = kv.key;
        json collection_structure = kv.value;

        if(collection_name.empty()){
            throw runtime_error("Пустое имя коллекции в schema.json");
        }

        Collection coll(collection_name, base_path, schema.tuples_limit, collection_structure);
        collections.insert(collection_name, coll);

        ++it;
    }
}

Collection& Database::get_collection(const string& name){
    if(!collections.contains(name)){
        throw runtime_error("Коллекция " + name + " не найдена");
    }
    return collections[name];
}

Vector<string> Database::get_collection_names() const{
    Vector<string> names;
    auto it = collections.begin();
    auto end_it = collections.end();
    while(it != end_it){
        auto kv = *it;
        names.push_back(kv.key);
        ++it;
    }
    return names;
}

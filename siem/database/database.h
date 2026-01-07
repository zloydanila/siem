#pragma once
#include <string>
#include "../containers/vector.h"
#include "../containers/hash_map.h"
#include "../schema/schema.h"
#include "../collection/collection.h"

using namespace std;

class Database {
private:
    Schema schema;
    string base_path;
    HashMap<string, Collection> collections;

public:
    Database(const string& schema_file, const string& db_name, const string& data_root);
    Collection& get_collection(const string& name);
    Vector<string> get_collection_names() const;
    string get_base_path() const { return base_path; }
};

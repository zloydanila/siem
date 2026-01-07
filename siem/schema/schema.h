#pragma once
#include <string>
#include "../containers/vector.h"
#include "../containers/hash_map.h"
#include "../include/json.hpp"

using namespace std;

struct Schema {
    string name;
    int tuples_limit;
    HashMap<string, nlohmann::json> structure;
    
    Schema() : tuples_limit(1000) {}
    Schema(const Schema& other) : name(other.name), tuples_limit(other.tuples_limit), structure(other.structure) {}
    Schema& operator=(const Schema& other) {
        if (this != &other) {
            name = other.name;
            tuples_limit = other.tuples_limit;
            structure = other.structure;
        }
        return *this;
    }
};

Schema load_schema(const string& filename);
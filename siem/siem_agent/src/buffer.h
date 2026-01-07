#pragma once
#include "../../containers/vector.h"
#include "../../include/json.hpp"
#include <mutex>
#include <string>

using json = nlohmann::json;
using namespace std;

class EventBuffer {
private:
    Vector<json> events;
    unsigned int head;
    unsigned int count;
    unsigned int capacity;

    string spool_path;
    mutable mutex m;

    void spool_append_nolock(const json& e);
    bool spool_pop_one_nolock(json& out);
    void ensure_dir_nolock(const string& dir);

public:
    EventBuffer(unsigned int cap = 4096, const string& spool = "siem_agent_spool.ndjson");

    void push(const json& e);
    Vector<json> pop_batch(int max_count);
    unsigned int size() const;
    void push_front_spool(const Vector<json>& batch);
};

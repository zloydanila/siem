#pragma once
#include <string>
#include "buffer.h"
#include "config.h"
#include "../../include/json.hpp"

using json = nlohmann::json;
using namespace std;

class LogCollector{
private:
    AgentConfig cfg;
    EventBuffer* buffer;

public:
    LogCollector(const AgentConfig& config, EventBuffer* buf);
    void run_file(const string& source_name, const string& path);
    static string source_to_path(const string& src);
    static string guess_source_name(const string& src);
};

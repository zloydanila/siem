#pragma once
#include "../../containers/vector.h"
#include "../../include/json.hpp"
#include <string>

using json = nlohmann::json;
using namespace std;

struct AgentConfig{
    string agentid;
    string server_host;
    int server_port;
    int sendinterval;
    int batchsize;
    Vector<string> sources;

    void load(const string& path);
};

#include "config.h"
#include <fstream>
#include <stdexcept>

void AgentConfig::load(const string& path){
    ifstream file(path);
    if(!file.is_open()){
        throw runtime_error("ошибка открытия конфига: " + path);
    }

    json config;
    try{
        file >> config;
    }catch(const json::parse_error& e){
        throw runtime_error("ошибка json в конфиге: " + string(e.what()));
    }

    if(!config.contains("agentid") || !config["agentid"].is_string()){
        throw runtime_error("agentid обязан быть в конфиге");
    }

    agentid = config["agentid"].get<string>();
    server_host = config.value("server_host", string("127.0.0.1"));
    server_port = config.value("server_port", 5140);
    sendinterval = config.value("sendinterval", 30);
    batchsize = config.value("batchsize", 100);

    sources.clear();

    if(config.contains("sources") && config["sources"].is_array()){
        for(const auto& src : config["sources"]){
            if(src.is_string()){
                sources.push_back(src.get<string>());
            }
        }
    }
}

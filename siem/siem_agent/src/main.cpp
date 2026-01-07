#include <thread>
#include <chrono>
#include <iostream>

#include "config.h"
#include "utils.h"
#include "sender.h"
#include "buffer.h"
#include "log_collector.h"
#include "../../include/json.hpp"

using namespace std;
using json = nlohmann::json;

static void sender_thread_func(AgentConfig config, EventBuffer* buffer){
    Sender sender(config.server_host, config.server_port);

    while(true){
        Vector<json> batch = buffer->pop_batch(config.batchsize);

        if(batch.get_size() == 0){
            this_thread::sleep_for(chrono::seconds(config.sendinterval));
            continue;
        }

        json packet;
        packet["agentid"] = config.agentid;
        packet["timestamp"] = get_timestamp();
        packet["events"] = json::array();

        for(unsigned int i = 0; i < batch.get_size(); i++){
            packet["events"].push_back(batch[i]);
        }

        if(!sender.send_line(packet.dump())){
            buffer->push_front_spool(batch);
            this_thread::sleep_for(chrono::seconds(1));
            continue;
        }

        string resp;
        bool got = sender.recv_line(resp, 2000);
        if(!got){
            buffer->push_front_spool(batch);
            this_thread::sleep_for(chrono::seconds(1));
            continue;
        }

        if(resp != "OK"){
            buffer->push_front_spool(batch);
            this_thread::sleep_for(chrono::seconds(1));
            continue;
        }

        this_thread::sleep_for(chrono::seconds(config.sendinterval));
    }
}

int main(){
    try{
        AgentConfig config;
        config.load("../siem_agent/agent.conf");

        EventBuffer buffer(2, "/tmp/siem_events.ndjson");

        thread s(sender_thread_func, config, &buffer);

        LogCollector collector(config, &buffer);
        Vector<thread*> collectors;

        for(unsigned int i = 0; i < config.sources.get_size(); i++){
            string src = config.sources[i];
            string path = LogCollector::source_to_path(src);
            if(path.empty()) continue;

            thread* t = new thread(&LogCollector::run_file, &collector, src, path);
            collectors.push_back(t);
        }

        s.join();
        for(unsigned int i = 0; i < collectors.get_size(); i++){
            collectors[i]->join();
            delete collectors[i];
        }
    } catch(const exception& e){
        cerr << "ошибка: " << e.what() << endl;
        return 1;
    }

    return 0;
}

#include "utils.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <unistd.h>
#include <ctime>

string get_timestamp(){
    auto now = chrono::system_clock::now();
    auto time_t_now = chrono::system_clock::to_time_t(now);

    stringstream ss;
    ss << put_time(gmtime(&time_t_now), "%Y-%m-%dT%H%M%SZ");
    return ss.str();
}

string get_hostname_str(){
    char buf[128];
    if(gethostname(buf, sizeof(buf)) == 0){
        return string(buf);
    }
    return "unknown";
}

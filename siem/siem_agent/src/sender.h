#pragma once
#include <string>

using namespace std;

class Sender{
private:

    int sock_fd;
    string host;
    int port;

    void close_socket();

public:
    Sender(const string& host, int port);
    ~Sender();

    bool connect_to_server();
    bool ensure_connected();
    bool send_line(const string& line);
    bool recv_line(string& out, int timeout_ms);
};

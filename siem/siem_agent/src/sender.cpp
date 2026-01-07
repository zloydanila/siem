#include "sender.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>

Sender::Sender(const string& h, int p) : sock_fd(-1), host(h), port(p){
}

Sender::~Sender(){
    close_socket();
}

void Sender::close_socket(){
    if(sock_fd >= 0){
        close(sock_fd);
        sock_fd = -1;
    }
}

bool Sender::connect_to_server(){
    close_socket();

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(sock_fd < 0) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if(inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0){
        close_socket();
        return false;
    }

    if(connect(sock_fd, (sockaddr*)&addr, sizeof(addr)) < 0){
        close_socket();
        return false;
    }

    return true;
}

bool Sender::ensure_connected(){
    if(sock_fd >= 0) return true;
    return connect_to_server();
}

bool Sender::send_line(const string& line){
    if(!ensure_connected()) return false;

    string data = line + "\n";
    size_t total = 0;

    while(total < data.size()){
        ssize_t sent = send(sock_fd, data.data() + total, data.size() - total, 0);
        if(sent <= 0){
            close_socket();
            return false;
        }
        total += (size_t)sent;
    }
    return true;
}

bool Sender::recv_line(string& out, int timeout_ms){
    out.clear();
    if(!ensure_connected()) return false;

    while(true){
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(sock_fd, &rfds);

        timeval tv{};
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        int r = select(sock_fd + 1, &rfds, nullptr, nullptr, &tv);
        if(r <= 0){
            close_socket();
            return false;
        }

        char c;
        ssize_t n = recv(sock_fd, &c, 1, 0);
        if(n <= 0){
            close_socket();
            return false;
        }

        if(c == '\n') break;
        out.push_back(c);

        if(out.size() > 4096){
            close_socket();
            return false;
        }
    }

    return true;
}

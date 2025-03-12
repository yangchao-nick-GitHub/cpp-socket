#include <iostream>
#include <memory>
#include <csignal>

#include "socket.h"


void signalHandler(int signum) {
    LOG_ERROR("Interrupt signal (" + std::to_string(signum) + ") received.");
    running = false;
}


int main() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::shared_ptr<net::ServerSocket> server = std::make_shared<net::ServerSocket>();
    std::shared_ptr<net::InetAddress> addr = std::make_shared<net::IPV4InetAddress>("127.0.0.1", 8888);

    if (!server->setupListeningSocket(addr)) {
        LOG_ERROR("socket setup error");
        return -1;
    }

    std::shared_ptr<net::Epoll> epoll = std::make_shared<net::Epoll>();
    std::shared_ptr<net::Channel> server_chn = std::make_shared<net::Channel>(epoll, server->getFd());
    server_chn->enableReading();

    while (running) {
        std::vector<net::Channel*> active_chns;
        active_chns = epoll->wait();
        for (auto chn : active_chns) {
            if (chn->getFd() == server->getFd()) {
                std::shared_ptr<net::InetAddress> cnt_addr = std::make_shared<net::IPV4InetAddress>();
                int cnt_fd = server->accept(cnt_addr);
                if (cnt_fd == -1) {
                    LOG_ERROR("socket accept error");
                    continue;
                }
                LOG_INFO("new connection from: " + cnt_addr->ToString() + " fd: " + std::to_string(cnt_fd));
                std::shared_ptr<net::Channel> new_client_chn = std::make_shared<net::Channel>(epoll, cnt_fd);
                new_client_chn->enableReading();
            } else {
                activeFdServerHandle(chn->getFd());
            }
        }
    }

    return 0;
}

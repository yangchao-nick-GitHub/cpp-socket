#include <iostream>
#include <memory>

#include "socket.h"



int main() {
    std::shared_ptr<net::ServerSocket> server = std::make_shared<net::ServerSocket>();
    std::shared_ptr<net::InetAddress> addr = std::make_shared<net::IPV4InetAddress>("127.0.0.1", 8888);

    if (!server->setupListeningSocket(addr)) {
        std::cerr << "socket setup error" << std::endl;
        return -1;
    }

    std::vector<int> active_fds;
    std::shared_ptr<net::Epoll> epoll = std::make_shared<net::Epoll>();
    epoll->add(server->getFd(), EPOLLIN);
    std::shared_ptr<net::ActiveFdHandler> active_handle = std::make_shared<net::defaultServerActiveFdHandler>();
    while (true) {
        active_fds = epoll->wait();
        for (auto fd : active_fds) {
            if (fd == server->getFd()) {
                std::shared_ptr<net::InetAddress> cnt_addr = std::make_shared<net::IPV4InetAddress>();
                int cnt_fd = server->accept(cnt_addr);
                if (cnt_fd == -1) {
                    std::cerr << "socket accept error" << std::endl;
                    continue;
                }
                std::cout << "new connection from: " << cnt_addr->ToString() << " fd: " << cnt_fd << std::endl;
                net::setNonBlocking(cnt_fd);
                epoll->add(cnt_fd, EPOLLIN | EPOLLET);
            } else {
                active_handle->handle(fd);
            }
        }
    }

    return 0;
}

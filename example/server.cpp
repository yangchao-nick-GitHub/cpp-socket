#include <iostream>
#include <memory>
#include "socket.h"


int main() {
    std::shared_ptr<net::Server> server = std::make_shared<net::Server>("127.0.0.1", 5000);
    server->setDataUserCallback([](int fd, char* data, size_t len) {
        std:: cout << "user : " << data << std::endl;
    });
    server->start();
    return 0;
}

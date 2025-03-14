#include <iostream>
#include <memory>
#include "socket.h"


int main() {
    std::shared_ptr<net::Server> server = std::make_shared<net::Server>("127.0.0.1", 5000);
    server->start();
    return 0;
}

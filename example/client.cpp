#include <iostream>
#include <memory>

#include "socket.h"

int main() {
    auto socket = std::make_shared<net::ClientSocket>();
    auto addr = std::make_shared<net::IPV4InetAddress>("127.0.0.1", 8888);
    if (!socket->connect(addr)) {
        LOG_ERROR("socket connect error");
        return -1;
    }
    char buffer[1024];
    while(true) {
        bzero(&buffer, sizeof(buffer));
        if (fgets(buffer, sizeof(buffer), stdin) == nullptr) {
            break;
        }
        buffer[strcspn(buffer, "\n")] = '\0';

        int write_bytes = send(socket->getFd(), &buffer[0], strlen(buffer), 0);
        if (write_bytes == -1) {
            break;
        }
    }

    return 0;
}

#include <iostream>
#include <memory>

#include "socket.h"

int main() {
    std::shared_ptr<net::ClientSocket> socket = std::make_shared<net::ClientSocket>();
    std::shared_ptr<net::InetAddress> addr = std::make_shared<net::IPV4InetAddress>("127.0.0.1", 8888);
    if (!socket->connect(addr)) {
        std::cerr << "socket connect error" << std::endl;
        return -1;
    }
    char buffer[1024];
    while(true) {
        bzero(&buffer, sizeof(buffer));
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
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

#include <iostream>
#include <memory>
#include <string.h>
#include <signal.h>

#include "socket.h"



void handleSingal(int sig) 
{
    std::cout << "recv ctrl c singal, exit" << std::endl;
    exit(0);
}

int main() {
    /* handle ctrl c singal  */
    signal(SIGINT, handleSingal);

    auto socket = std::make_shared<net::ClientSocket>();
    auto addr = std::make_shared<net::IPV4InetAddress>("127.0.0.1", 5000);
    if (!socket->connect(addr)) {
        perror("socket connect error");
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

#ifndef __SOCKET_UTIL_H__
#define __SOCKET_UTIL_H__
#include <memory>
#include <stdexcept>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>

#define LOG_INFO(msg) std::cout << "[INFO] " << msg << std::endl
#define LOG_ERROR(msg) std::cerr << "[ERROR] " << msg << std::endl

namespace net {


void setNonBlocking(int fd)
{
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

class InetAddress {
public:
    virtual ~InetAddress() = default;
    virtual bool AddressValid(const std::string& ip, uint16_t port) = 0;
    virtual std::string ToString() = 0;
    virtual struct sockaddr_in& getSockAddr() = 0;
    virtual socklen_t getAddrLen() const = 0;
};

class IPV4InetAddress : public InetAddress {
public:
    IPV4InetAddress(): addr_len(sizeof(serv_addr)) {}
    IPV4InetAddress(const std::string& ip, uint16_t port): addr_len(sizeof(serv_addr))
    {
        if (!AddressValid(ip, port)) {
            throw std::invalid_argument("invalid ip or port");
        }
        bzero(&serv_addr, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = inet_addr(ip.c_str());
        serv_addr.sin_port = htons(port);
    }

    bool AddressValid(const std::string& ip, uint16_t port) override
    {
        if (port < 0 || port > 65535) {
            return false;
        }
        struct sockaddr_in addr4;
        if (inet_pton(AF_INET, ip.c_str(), &addr4.sin_addr) > 0) {
            return true;
        }
        return false;
    }

    std::string ToString() override
    {
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &serv_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
        return std::string(ip_str) + ":" + std::to_string(ntohs(serv_addr.sin_port));
    }

    struct sockaddr_in& getSockAddr()
    {
        return serv_addr;
    }

    socklen_t getAddrLen() const
    {
        return addr_len;
    }
private:
    struct sockaddr_in serv_addr;
    socklen_t addr_len;
};

class Socket {
public:
    Socket()
    {
        sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd_ == -1) {
            throw std::runtime_error("socket create error");
            return;
        }
    }

    virtual ~Socket()
    {
        close(sockfd_);
    }

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    int getFd() const
    {
        return sockfd_;
    }

protected:
    int sockfd_;
};

class ServerSocket : public Socket {
public:
    ServerSocket(): Socket() {}

    bool setupListeningSocket(std::shared_ptr<InetAddress> addr, bool is_reuse_addr = true)
    {
        if (is_reuse_addr) {
            int opt = 1;
            if (setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
                return false;
            }
        }
        
        if (bind(sockfd_, (sockaddr*)&(addr->getSockAddr()), addr->getAddrLen()) == -1) {
            return false;
        }
        if (listen(sockfd_, SOMAXCONN) == -1) {
            return false;
        }
        return true;
    }

    int accept(std::shared_ptr<InetAddress> addr)
    {
        struct sockaddr_in& client_addr = addr->getSockAddr();
        socklen_t addr_len = addr->getAddrLen();
        return ::accept(sockfd_, (sockaddr*)&client_addr, &addr_len);
    }
};

class ClientSocket : public Socket {
public:
    ClientSocket(): Socket() {}

    bool connect(std::shared_ptr<InetAddress> addr)
    {
        struct sockaddr_in client_addr = addr->getSockAddr();
        socklen_t addr_len = addr->getAddrLen();
        return ::connect(sockfd_, (sockaddr*)&client_addr, addr_len) == 0;
    }
};

class Epoll {
public:

    Epoll()
    {
        epfd_ = epoll_create(1);
        if (epfd_ == -1) {
            throw std::runtime_error("epoll create error");
        }
        bzero(&event, sizeof(event));
    }
    ~Epoll()
    {
        if (epfd_ != -1) {
            close(epfd_);
        }
    }

    Epoll(const Epoll&) = delete;
    Epoll& operator=(const Epoll&) = delete;

    int getFd() const
    {
        return epfd_;
    }

    int add(int fd, uint32_t  opt)
    {
        struct epoll_event ev;
        ev.events = opt;
        ev.data.fd = fd;
        return epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev);
    }

    std::vector<int> wait(int timeout = -1)
    {
        std::vector<int> active_fds;
        int fds = epoll_wait(epfd_, event, 1024, timeout);
        for (int i = 0; i < fds; i++) {
            active_fds.push_back(event[i].data.fd);
        }
        return active_fds;
    }

private:
    int epfd_;
    struct epoll_event event[1024];
};

class ActiveFdHandler {
public:
    virtual ~ActiveFdHandler() = default;
    virtual void handle(int fd) = 0;
};

class defaultServerActiveFdHandler : public ActiveFdHandler {
public:
    void handle(int fd) override
    {
        char buffer[1024];
        while (true) {
            bzero(&buffer, sizeof(buffer));
            ssize_t read_bytes = recv(fd, buffer, sizeof(buffer) - 1, 0);
            if (read_bytes > 0) {
                LOG_INFO("message from client fd: " + std::to_string(fd) + " message: " + buffer);
            } else if (read_bytes == 0) {
                LOG_INFO("client disconnected fd:" + std::to_string(fd));
                break;
            } else if (read_bytes == -1 && errno == EINTR) {
                continue;  // 信号中断，继续读取
            } else if (read_bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                break;  // 非阻塞模式下暂时无数据，退出读取循环
            } else {
                break;
            }
        }
    }
};

}

#endif
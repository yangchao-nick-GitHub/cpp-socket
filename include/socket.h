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

void setNonBlocking(int fd);

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
    IPV4InetAddress();
    IPV4InetAddress(const std::string& ip, uint16_t port);
    bool AddressValid(const std::string& ip, uint16_t port) override;
    std::string ToString() override;
    struct sockaddr_in& getSockAddr();
    socklen_t getAddrLen() const;
private:
    struct sockaddr_in serv_addr;
    socklen_t addr_len;
};

class Socket {
public:
    Socket();
    virtual ~Socket();
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    int getFd() const;
protected:
    int sockfd_;
};

class ServerSocket : public Socket {
public:
    ServerSocket();
    bool setupListeningSocket(std::shared_ptr<InetAddress> addr, bool is_reuse_addr = true);
    int accept(std::shared_ptr<InetAddress> addr);
};

class ClientSocket : public Socket {
public:
    ClientSocket();
    bool connect(std::shared_ptr<InetAddress> addr);
};

class Channel;

class Epoll {
public:
    Epoll();
    ~Epoll();
    Epoll(const Epoll&) = delete;
    int getFd() const;
    int add(int fd, uint32_t  opt);
    std::vector<Channel*> wait(int timeout = 1);
    void updateChannel(Channel& channel);
private:
    int epfd_;
    struct epoll_event event[1024];
};

class Channel
{
private:
    std::shared_ptr<Epoll> epoll;
    int monitor_fd;
    uint32_t monitor_events;
    uint32_t revents;
    bool inEpoll {false};
public:
    Channel(std::shared_ptr<Epoll> epoll, int fd);
    ~Channel() = default;

    void enableReading();
    int getFd();
    uint32_t getEvents();
    bool getInEpoll();
    void setInEpoll();
    uint32_t getRevents();
    void setRevents(uint32_t revents);
};

}

#endif
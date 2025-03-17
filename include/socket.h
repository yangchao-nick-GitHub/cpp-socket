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
#include <string>
#include <sys/epoll.h>
#include <functional>
#include <unordered_map>
#include <atomic>

#include "task_queue.h"

#define LOG_INFO(msg) std::cout << "[INFO] " << msg << std::endl
#define LOG_ERROR(msg) std::cerr << "[ERROR] " << msg << std::endl

namespace net {

class Buffer {
public:
    Buffer() = default;
    ~Buffer() = default;
    void append(const char* data, size_t len);
    const char* cStr();
    size_t size();
    void clear();
private:
    std::string buffer;
};
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
    explicit  IPV4InetAddress(const std::string& ip, uint16_t port);
    bool AddressValid(const std::string& ip, uint16_t port) override;
    std::string ToString() override;
    struct sockaddr_in& getSockAddr();
    socklen_t getAddrLen() const override;
private:
    struct sockaddr_in serv_addr {};
    socklen_t addr_len;
};

class SocketUtils {
public:
    static void setNonBlocking(int fd);
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
    void addEvent(std::shared_ptr<Channel> channel);
    void delEvent(Channel& channel);
private:
    int epfd_;
    struct epoll_event event[1024];
};

class EventLoop;

class Channel
{
protected:
    using ChannelCallback = std::function<void()>;
    int monitor_fd;
    uint32_t monitor_events;
    uint32_t revents;
    bool is_in_epoll {false};
public:
    Channel(int fd);
    virtual ~Channel() = default;
    virtual void setEvent() = 0;
    uint32_t getEvents();

    void setRevents(uint32_t revents);
    uint32_t getRevents();
    int getFd();
    bool getInEpoll();
    void setInEpoll(bool is_in_epoll);
    virtual ChannelCallback getChannelWorkCallback() = 0;

    
};


class Buffer;

class AcceptChannel : public Channel {
public:
    using AcceptCallback = std::function<void()>;

    AcceptChannel(int fd);
    void setEvent() override;
    void setAcceptCallback(AcceptCallback cb);
    void handleAccept();
    ChannelCallback getChannelWorkCallback() override;
private:
    AcceptCallback accept_cb;
};

class ConnectionChannel : public Channel {
public:
    using DataCallback  = std::function<void(int, char*, size_t)>;
    using CloseCallback = std::function<void(int)>;

    ConnectionChannel(int fd);
    void setEvent() override;
    void setDataCallback(DataCallback cb);
    void setCloseCallback(CloseCallback cb);
    void handleClose();
    void handleConnection();
    ChannelCallback getChannelWorkCallback() override;
    void echo(int fd, char* msg, size_t size);
private:
    DataCallback data_cb;
    CloseCallback close_cb;
};

class EventLoop {
public:
    EventLoop();
    ~EventLoop() = default;
    void loop();
    void addEvent(std::shared_ptr<Channel> chn);
    void updateChannel(Channel& channel);
private:
    std::shared_ptr<Epoll> epoll;
    std::atomic<bool> quite {false};
    std::shared_ptr<WorkQueue> wq;
};

    
class Server {
public:
    using DataHandlerFunc = std::function<void(int, char*, size_t)>;
    Server(std::string ip, uint16_t port);
    ~Server() = default;
    void handleNewConnection();
    void handleClientDisconnect(int fd);
    void setDataUserCallback(std::function<void(int, char*, size_t)> cb);
    void start();

private:
    std::shared_ptr<EventLoop> event_loop;  
    std::shared_ptr<ServerSocket> server_socket;
    std::shared_ptr<AcceptChannel> accept;
    DataHandlerFunc user_data_cb {nullptr};
    std::unordered_map<int, std::shared_ptr<ConnectionChannel>> connections;
};

};

#endif
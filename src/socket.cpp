#include "socket.h"

namespace net {

void setNonBlocking(int fd)
{
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

IPV4InetAddress::IPV4InetAddress(): addr_len(sizeof(serv_addr)) {}

IPV4InetAddress::IPV4InetAddress(const std::string& ip, uint16_t port): addr_len(sizeof(serv_addr))
{
    if (!AddressValid(ip, port)) {
        throw std::invalid_argument("invalid ip or port");
    }
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip.c_str());
    serv_addr.sin_port = htons(port);
}

bool IPV4InetAddress::AddressValid(const std::string& ip, uint16_t port)
{
    return (port <= 65535) && (inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr) > 0);
}

std::string IPV4InetAddress::ToString()
{
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &serv_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
    return std::string(ip_str) + ":" + std::to_string(ntohs(serv_addr.sin_port));
}

struct sockaddr_in& IPV4InetAddress::getSockAddr()
{
    return serv_addr;
}

socklen_t IPV4InetAddress::getAddrLen() const
{
    return addr_len;
}

Socket::Socket()
{
    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_ == -1) {
        throw std::runtime_error("socket create error");
        return;
    }
}

Socket::~Socket()
{
    close(sockfd_);
}

int Socket::getFd() const
{
    return sockfd_;
}

ServerSocket::ServerSocket(): Socket() {}

bool ServerSocket::setupListeningSocket(std::shared_ptr<InetAddress> addr, bool is_reuse_addr)
{
    if (addr == nullptr) {
        return false;
    }

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

int ServerSocket::accept(std::shared_ptr<InetAddress> addr)
{
    struct sockaddr_in& client_addr = addr->getSockAddr();
    socklen_t addr_len = addr->getAddrLen();
    return ::accept(sockfd_, (sockaddr*)&client_addr, &addr_len);
}

ClientSocket::ClientSocket(): Socket() {}

bool ClientSocket::connect(std::shared_ptr<InetAddress> addr)
{
    struct sockaddr_in client_addr = addr->getSockAddr();
    socklen_t addr_len = addr->getAddrLen();
    return ::connect(sockfd_, (sockaddr*)&client_addr, addr_len) == 0;
}

Epoll::Epoll()
{
    epfd_ = epoll_create(1);
    if (epfd_ == -1) {
        throw std::runtime_error("epoll create error");
    }
    bzero(&event, sizeof(event));
}

Epoll::~Epoll()
{
    if (epfd_ != -1) {
        close(epfd_);
    }
}

int Epoll::getFd() const
{
    return epfd_;
}

int Epoll::add(int fd, uint32_t  opt)
{
    struct epoll_event ev;
    ev.events = opt;
    ev.data.fd = fd;
    return epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev);
}

std::vector<Channel*> Epoll::wait(int timeout)
{
    std::vector<Channel*> active_channels;
    int fds = epoll_wait(epfd_, event, 1024, timeout);
    for (int i = 0; i < fds; i++) {
        Channel* avtive_ch = (Channel*)event[i].data.ptr;
        avtive_ch->setRevents(event[i].events);
        active_channels.push_back(avtive_ch);
    }
    return active_channels;
}

void Epoll::updateChannel(Channel& channel)
{
    struct epoll_event ev;
    ev.events = channel.getEvents();
    ev.data.fd = channel.getFd();
    ev.data.ptr = &channel;
    if (!channel.getInEpoll()) {
        epoll_ctl(epfd_, EPOLL_CTL_ADD, channel.getFd(), &ev);
    } else {
        epoll_ctl(epfd_, EPOLL_CTL_MOD, channel.getFd(), &ev);
    }
    channel.setInEpoll();
}

Channel::Channel(std::shared_ptr<EventLoop> event_loop, int fd)
{
    if (event_loop == nullptr) {
        throw std::invalid_argument("event_loop is null");
    }
    this->event_loop = event_loop;
    this->monitor_fd = fd;
    monitor_events = 0;
}

void Channel::enableReading()
{
    monitor_events = EPOLLIN | EPOLLET;
    setNonBlocking(monitor_fd);
    event_loop->updateChannel(*this);
}

int Channel::getFd()
{
    return monitor_fd;
}

uint32_t Channel::getEvents()
{
    return monitor_events;
}

uint32_t Channel::getRevents()
{
    return revents;
}

bool Channel::getInEpoll()
{
    return inEpoll;
}

void Channel::setInEpoll()
{
    inEpoll = true;
}

void Channel::setRevents(uint32_t revents)
{
    this->revents = revents;
}

void Channel::setCallback(std::function<void()> callback)
{
    this->callback = callback;
}

void Channel::handleEvent()
{
    callback();
}

EventLoop::EventLoop()
{
    epoll = std::make_shared<Epoll>();
    quite.store(true);
}

void EventLoop::loop()
{
    while (quite.load()) {
        std::vector<Channel*> active_chns;
        active_chns = epoll->wait();
        for (auto chn : active_chns) {
            chn->handleEvent();
        }
    }
}

void EventLoop::updateChannel(Channel& channel)
{
    epoll->updateChannel(channel);
}


Server::Server(std::string ip, uint16_t port)
{
    server_socket = std::make_shared<ServerSocket>();
    if (server_socket == nullptr) {
        throw std::runtime_error("server_socket create error");
    }

    event_loop = std::make_shared<EventLoop>();
    if (event_loop == nullptr) {
        throw std::runtime_error("event_loop create error");
    }

    if (!server_socket->setupListeningSocket(std::make_shared<IPV4InetAddress>(ip, port), true)) {
        throw std::runtime_error("socket setup error");
    }
    
    auto sever_chn = std::make_shared<Channel>(event_loop, server_socket->getFd());
    std::function<void()> callback = std::bind(&Server::handleNewConnection, this);
    sever_chn->setCallback(callback);
    sever_chn->enableReading();
    channels.emplace(server_socket->getFd(), sever_chn);
}

void Server::handleNewConnection()
{
    auto cnt_addr = std::make_shared<IPV4InetAddress>();
    int cnt_fd = server_socket->accept(cnt_addr);
    if (cnt_fd == -1) {
        LOG_ERROR("socket accept error");
    }
    LOG_INFO("new connection from: " + cnt_addr->ToString() + " fd: " + std::to_string(cnt_fd));
    auto client_chn = std::make_shared<Channel>(event_loop, cnt_fd);
    client_chn->setCallback(std::bind(&Server::handleActiveConnection, this, cnt_fd));
    client_chn->enableReading();
    channels.emplace(cnt_fd, client_chn);
}

void Server::handleClientDisconnect(int fd)
{
    LOG_INFO("client disconnected fd:" + std::to_string(fd));
    auto it = channels.find(fd);
    if (it!= channels.end()) {
        channels.erase(fd);
    }
}

void Server::handleActiveConnection(int fd)
{
    char buffer[1024];
    while (true) {
        bzero(&buffer, sizeof(buffer));
        ssize_t read_bytes = recv(fd, buffer, sizeof(buffer) - 1, 0);
        if (read_bytes > 0) {
            buffer[read_bytes] = '\0';
            LOG_INFO("message from client fd: " + std::to_string(fd) + " message: " + buffer);
        } else if (read_bytes == 0) {
            handleClientDisconnect(fd);
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

void Server::start()
{
    event_loop->loop();
}
    

}
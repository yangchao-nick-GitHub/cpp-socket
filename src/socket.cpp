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

void Epoll::addEvent(std::shared_ptr<Channel> chn)
{
    struct epoll_event ev;
    ev.events = chn->getEvents();
    ev.data.fd = chn->getFd();
    ev.data.ptr = chn.get();
        
    if (!chn->getInEpoll()) {
        epoll_ctl(epfd_, EPOLL_CTL_ADD, chn->getFd(), &ev);
    } else {
        epoll_ctl(epfd_, EPOLL_CTL_MOD, chn->getFd(), &ev);
    }
    
    chn->setInEpoll(false);
}

void Epoll::delEvent(Channel& channel)
{
    epoll_ctl(epfd_, EPOLL_CTL_DEL, channel.getFd(), nullptr);
    channel.setInEpoll(false);
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
    channel.setInEpoll(true);
}

Channel::Channel(int fd, std::function<void()> cb)
{
    this->monitor_fd = fd;
    this->event_cb = cb;
    monitor_events = 0;
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
    return is_in_epoll;
}

void Channel::setInEpoll(bool is_in_epoll)
{
    this->is_in_epoll = is_in_epoll;
}

void Channel::setRevents(uint32_t revents)
{
    this->revents = revents;
}

void Channel::setCallback(std::function<void()> callback)
{
    this->event_cb = callback;
}

void Channel::handleEvent()
{
    event_cb();
}

std::function<void()> Channel::getEventCallBack()
{
    return event_cb;
}

void Channel::setNonBlocking()
{
    fcntl(monitor_fd, F_SETFL, fcntl(monitor_fd, F_GETFL) | O_NONBLOCK);
}

AcceptChannel::AcceptChannel(int fd, std::function<void()> cb):Channel(fd, cb)
{
    setEvent();
    setNonBlocking();
}

void AcceptChannel::setEvent()
{
    /* setup event */
    monitor_events = EPOLLIN | EPOLLET;
}

ConnectionChannel::ConnectionChannel(int fd, std::function<void()> cb):Channel(fd, cb)
{
    setEvent();
    setNonBlocking();
}

void ConnectionChannel::setEvent()
{
    /* setup event */
    monitor_events = EPOLLIN | EPOLLET;
}


EventLoop::EventLoop()
{
    wq = std::make_shared<WorkQueue>(2, 10);
    epoll = std::make_shared<Epoll>();
    quite.store(true);
}

void EventLoop::loop()
{
    while (quite.load()) {
        std::vector<Channel*> active_chns;
        active_chns = epoll->wait();
        for (auto chn : active_chns) {
            wq->submit(std::move(chn->getEventCallBack()));
        }
    }
}

void EventLoop::updateChannel(Channel& channel)
{
    epoll->updateChannel(channel);
}

void EventLoop::addEvent(std::shared_ptr<Channel> chn)
{
    epoll->addEvent(chn);
}

void Buffer::append(const char* data, size_t len)
{
    for(int i = 0; i < len; ++i){
        if(data[i] == '\0') break;
        buffer.push_back(data[i]);
    }
}

const char* Buffer::cStr()
{
    return buffer.c_str();
}

size_t Buffer::size()
{
    return buffer.size();
}

void Buffer::clear()
{
    buffer.clear();
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

    accept = std::make_shared<AcceptChannel>(server_socket->getFd(), std::bind(&Server::handleNewConnection, this));
    event_loop->addEvent(accept);
}

void Server::handleNewConnection()
{
    auto cnt_addr = std::make_shared<IPV4InetAddress>();
    int cnt_fd = server_socket->accept(cnt_addr);
    if (cnt_fd == -1) {
        LOG_ERROR("socket accept error");
    }
    LOG_INFO("new connection from: " + cnt_addr->ToString() + " fd: " + std::to_string(cnt_fd));

    auto client = std::make_shared<ConnectionChannel>(cnt_fd, std::bind(&Server::handleActiveConnection, this, cnt_fd));
    event_loop->addEvent(client);
    connections.emplace(cnt_fd, client);
}

void Server::handleClientDisconnect(int fd)
{
    auto it = connections.find(fd);
    if (it!= connections.end()) {
        LOG_INFO("client disconnected fd:" + std::to_string(fd));
        connections.erase(fd);
    }
}

void Server::handleActiveConnection(int fd)
{
    char buf[1024];
    while (true) {
        bzero(&buf, sizeof(buf));
        ssize_t read_bytes = recv(fd, buf, sizeof(buf) - 1, 0);
        if (read_bytes > 0) {
            buf[read_bytes] = '\0';
            buffer.append(buf, read_bytes);
        } else if (read_bytes == 0) {
            handleClientDisconnect(fd);
            break;
        } else if (read_bytes == -1 && errno == EINTR) {
            LOG_ERROR("recv message interrupted");
            continue;  // 信号中断，继续读取
        } else if (read_bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            LOG_INFO("message from client fd: " + std::to_string(fd) + " message: " + buffer.cStr());
            write(fd, buffer.cStr(), buffer.size());
            buffer.clear();
            break;  // 非阻塞模式下暂时无数据，退出读取循环
        } else {
            handleClientDisconnect(fd);
            break;
        }
    }
}

void Server::start()
{
    event_loop->loop();
}
    

}
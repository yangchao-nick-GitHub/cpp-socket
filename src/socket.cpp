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
    if (port < 0 || port > 65535) {
        return false;
    }
    struct sockaddr_in addr4;
    if (inet_pton(AF_INET, ip.c_str(), &addr4.sin_addr) > 0) {
        return true;
    }
    return false;
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

Channel::Channel(std::shared_ptr<Epoll> epoll, int fd)
{
    if (epoll == nullptr) {
        throw std::invalid_argument("epoll is null");
    }
    this->epoll = epoll;
    this->monitor_fd = fd;
    monitor_events = 0;
}

void Channel::enableReading()
{
    monitor_events = EPOLLIN | EPOLLET;
    setNonBlocking(monitor_fd);
    epoll->updateChannel(*this);
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
    

}
# C++ Socket 网络编程框架

这是一个基于 C++ 的轻量级 Socket 网络编程框架，支持服务器和客户端的通信。项目使用了 epoll 实现高效的 I/O 多路复用，并提供了简洁的 API 接口，方便开发者快速构建网络应用。

## 功能特性

- **高效 I/O 多路复用**：使用 epoll 实现高性能的网络通信
- **非阻塞模式**：支持非阻塞 Socket 操作，提高并发性能
- **简洁 API**：提供易于使用的接口，快速构建网络应用
- **IPv4 支持**：支持 IPv4 地址的解析和通信
- **多客户端连接**：服务器支持同时处理多个客户端连接

## 快速开始

### 编译项目

```bash
make

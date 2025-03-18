# C++ Socket 网络编程框架

这是一个基于 C++ 的轻量级 Socket 网络编程框架，支持服务器和客户端的通信。项目使用了 epoll 实现高效的 I/O 多路复用，参考了30dayMakeCppServer项目，并提供了更加简洁的 API 接口，内部面向接口编程，拓展性较好，方便开发者快速构建网络应用。

## 功能特性

- **高效 I/O 多路复用**：使用 epoll 实现高性能的网络通信
- **非阻塞模式**：支持非阻塞 Socket 操作，提高并发性能
- **简洁 API**：提供易于使用的接口，快速构建网络应用
- **自定义数据处理**：支持用户自定义数据处理逻辑
- **线程池支持**：内置线程池和工作队列，提高并发处理能力
- **C++11 特性**：充分利用 C++11 新特性，代码更简洁高效

## 核心组件
1. Socket 封装
   - `Socket`：基础 Socket 类
   - `ServerSocket`：服务器端 Socket 封装
   - `ClientSocket`：客户端 Socket 封装
2. 事件驱动
   - `Epoll`：epoll 封装，实现 I/O 多路复用
   - `Channel`：事件通道抽象
   - `EventLoop`：事件循环核心
3. 线程池
   - `WorkQueue`：工作队列实现
   - 支持自定义线程池大小

## 示例代码

### 服务端示例
```cpp
#include "server.h"

int main() {
    std::shared_ptr<net::Server> server = std::make_shared<net::Server>("127.0.0.1", 5000);
    server->setDataUserCallback([](int fd, char* data, size_t len) {
        std:: cout << "user : " << data << std::endl;
    });
    server->start();
    
    return 0;
}
```

### 客户端示例
```cpp
#include "client.h"

int main() {
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
```


## 快速开始

### 编译项目

```bash
mkdir build && cd build
cmake ../ && make
./server # 启动服务器
./client # 启动客户端




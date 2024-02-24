#pragma once
#include <sys/socket.h>   // 引入系统socket函数库，用于创建、绑定和监听套接字等操作
#include <sys/epoll.h>    // 引入epoll事件驱动机制的函数库，用于实现高效的IO多路复用
#include <fcntl.h>        // 引入文件描述符标志控制函数库，如fcntl()函数
#include <netinet/in.h>   // 引入IPv4地址族的网络编程接口，如sockaddr_in结构体
#include <unistd.h>       // 引入Unix标准函数库，提供close、read、write等基本系统调用
#include <cstring>        // 引入C字符串操作函数库
#include "ThreadPool.h"   // 引入线程池模块，用于并发处理客户端连接请求
#include "Router.h"       // 引入路由模块，根据HTTP请求的方法和路径分发到不同的处理器
#include "HttpRequest.h"  // 引入HTTP请求解析类，用于解析客户端发送过来的请求数据
#include "HttpResponse.h" // 引入HTTP响应构建类，用于构建服务端返回给客户端的响应数据
#include "Database.h"     // 引入库，提供与数据库交互的功能

class HttpServer
{
public:
    HttpServer(int port, int max_events, database &db)
        : server_fd(-1), epollfd(-1), PORT(port), MAX_EVENTS(max_events), db(db){};

    void setupRoutes()
    {
        router.addRoute("GET", "/", [](const HttpRequest& req) {
            HttpResponse response;
            response.setStatusCode(200);
            response.setBody("Hello, World!");
            return response;
        });
        
        router.setupDatabaseRoutes(db);
    }

    void start()
    {
        setupServerSocket();
        setupEpoll();
        ThreadPool pool(16);
        struct epoll_event events[MAX_EVENTS];
        while (true)
        {
            int nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
            for (int i = 0; i < nfds; i++)
            {
                if (events[i].data.fd == server_fd) // new connection arrive
                {
                    acceptConnection();
                }
                else
                {
                    int client_fd = events[i].data.fd;
                    pool.enqueue([client_fd, this]()
                                 { this->handleConnection(client_fd); });
                }
            }
        }
    }

private:
    int server_fd, epollfd, PORT, MAX_EVENTS;
    Router router;
    database &db;

    void setupServerSocket()
    {
        // create socket
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        LOG_INFO("Socket created!");
        // initialize server addresss
        struct sockaddr_in address;
        int addlen = sizeof(address);
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_family = AF_INET;
        address.sin_port = htons(PORT);
        // bind socket
        bind(server_fd, (struct sockaddr *)&address, addlen);
        // listen on socket
        listen(server_fd, 3);
        LOG_INFO("Listening on PORT %d", PORT);
    }

    void setupEpoll()
    {
        struct epoll_event ev;
        // create epoll instance
        epollfd = epoll_create1(0);
        if (epollfd == -1)
        {
            LOG_ERROR("epoll_create -1");
            exit(EXIT_FAILURE);
        }
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = server_fd;
        /*
        使用epoll_ctl函数将刚刚配置好的事件注册到epoll实例中。这里的参数意义如下：
        epollfd: 是之前创建的epoll实例的文件描述符；
        EPOLL_CTL_ADD: 指令标识，表示向epoll实例添加一个新的需要监控的文件描述符及其事件；
        server_fd: 需要监控的文件描述符，也就是服务器监听套接字；
        &ev: 指向包含事件类型和其他相关信息的epoll_event结构体的指针。
        */
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, server_fd, &ev) == -1)
        {
            LOG_ERROR("epoll_ctl: server_fd %d", server_fd);
            exit(EXIT_FAILURE);
        }
    }

    // 设置文件描述符为非阻塞模式的方法
    void setNonBlocking(int sock)
    {
        // 获取当前文件描述符的标志位
        int flags = fcntl(sock, F_GETFL, 0);

        // 设置O_NONBLOCK标志位
        flags |= O_NONBLOCK;

        // 更新文件描述符的标志位
        fcntl(sock, F_SETFL, flags);
    }

    void acceptConnection()
    {
        int new_socket;
        struct sockaddr_in address;
        socklen_t addlen = sizeof(address);
        // accept connection
        while ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addlen)) > 0)
        {
            setNonBlocking(new_socket);
            // 初始化一个新的epoll事件结构体ev，设置监听新连接的可读事件和边缘触发模式
            struct epoll_event ev = {};
            ev.events = EPOLLIN | EPOLLET;
            ev.data.fd = new_socket;
            if (epoll_ctl(epollfd, EPOLL_CTL_ADD, new_socket, &ev) == -1)
            {
                LOG_ERROR("epoll_ctl: new socket %d", new_socket);
                exit(EXIT_FAILURE);
            }
            else
            {
                LOG_INFO("New connection accepted, socket added to epoll");
            }
        }
        if (new_socket == -1 && (errno != EAGAIN && errno != EWOULDBLOCK))
        {
            LOG_ERROR("Error accepting new connection");
        }
    }
    // 读取请求、路由分发、生成响应并发送回客户端
    void handleConnection(int fd)
    {
        char buffer[4096];
        ssize_t bytes_read;
        while ((bytes_read = read(fd, buffer, sizeof(buffer) - 1)) > 0)
        {
            buffer[bytes_read] = '\0';

            // create HttpRequest instance
            HttpRequest request;
            if (request.parse(buffer))
            {
                // 根据HttpRequest对象通过Router对象获取对应的HttpResponse对象
                HttpResponse response = router.routeRequest(request);
                string response_str = response.toString();
                send(fd, response_str.c_str(), response_str.length(), 0);
            }
        }
        // 如果读取错误且错误原因不是EAGAIN，则打印错误信息
        if (bytes_read == -1 && errno != EAGAIN) {
            LOG_ERROR("Error reading from socket %d", fd);
        }

        // 处理完请求后关闭客户端连接
        close(fd);
    }
};

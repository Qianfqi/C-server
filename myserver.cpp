#include <iostream>
#include <map>
#include <functional>
#include <string>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <sstream>
#include "database.h"
#include "ThreadPool.h"
#define PORT 8080
#define MAX_EVENT 10
using namespace std;

using requesthandler = function<string(const string &)>;
map<string, requesthandler> get_table;
map<string, requesthandler> post_table;
database db("user.db"); // create database

void setuproutes()
{
    LOG_INFO("Setting up routes");
    get_table["/"] = [](const string &request)
    {
        return "hello world!";
    };
    get_table["/register"] = [](const string &request)
    {
        return "Please use POST to register";
    };
    get_table["/login"] = [](const string &request)
    {
        return "Please use POST to login";
    };
    post_table["/login"] = [](const string &request)
    {
        auto res = decodeBody(request);
        string username = res["username"];
        string password = res["password"];
        if (db.loginUser(username, password))
        {
            return "login success!";
        }
        return "login failed!";
    };
    post_table["/register"] = [](const string &request)
    {
        auto res = decodeBody(request);
        string username = res["username"];
        string password = res["password"];
        if (db.registerUser(username, password))
        {
            return "register success!";
        }
        return "register failed!";
    };
}

tuple<string, string, string> parseHttpRequest(const string &request)
{
    LOG_INFO("Parsing HTTP requests");
    size_t method_end = request.find(" ");
    string method = request.substr(0, method_end);
    size_t uri_end = request.find(" ", method_end + 1);
    string uri = request.substr(method_end + 1, uri_end - method_end - 1);
    string body;
    // as to POST, we need to analize the HTTP body
    if (method == "POST")
    {
        size_t body_start = request.find("\r\n\r\n");

        if (body_start != string::npos) // static const size_type npos = -1;
        {
            body = request.substr(body_start + 4);
        }
    }
    return {method, uri, body};
}

// as for the POST method, we need to anaylise the body
auto decodeBody(const string &body)
{
    map<string, string> params;
    istringstream stream(body);
    string pair;
    LOG_INFO("Decoding the body");
    while (getline(stream, pair, '&'))
    {
        size_t pos = pair.find("=");
        if (pos != string::npos)
        {
            string key = pair.substr(0, pos);
            string value = pair.substr(pos + 1);
            params[key] = value;
            LOG_INFO("Parsed key-value pair: %s = %s", key.c_str(), value.c_str()); // record every key-value pair
        }
        else
        {
            std::string error_msg = "Error parsing: " + pair;
            LOG_ERROR(error_msg.c_str()); // 记录错误信息
            std::cerr << error_msg << std::endl;
        }
    }
    return params;
}

string handlerequest(const string &method, const string &uri, const string &request)
{
    LOG_INFO("Handling HTTP request for URI: %s", uri.c_str());
    string response_body;
    if (method == "POST")
    {
        if (post_table.count(uri) == 0)
            response_body = "404 NOT FOUND!!!";
        else
            response_body = post_table[uri](request);
    }
    else if (method == "GET")
    {
        if (get_table.count(uri) == 0)
            response_body = "404 NOT FOUND!!!";
        else
            response_body = get_table[uri](request);
    }
    else
        response_body = "Method Wrong";
    return response_body;
}

void setNonBlocking(int sock)
{
    int opts;
    opts = fcntl(sock, F_GETFL); // 获取socket的文件描述符当前的状态标志
    if (opts < 0)
    {
        LOG_ERROR("fcntl(F_GETFL)"); // 获取标志失败，记录错误日志
        exit(EXIT_FAILURE);
    }
    opts = (opts | O_NONBLOCK); // 设置非阻塞标志
    if (fcntl(sock, F_SETFL, opts) < 0)
    {                                // 应用新的标志设置到socket
        LOG_ERROR("fcntl(F_SETFL)"); // 设置失败，记录错误日志
        exit(EXIT_FAILURE);
    }
}

int main()
{
    int server_fd, new_socket;
    int epollfd, nfds;
    struct epoll_event ev, events[MAX_EVENT];
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    LOG_INFO("Socket created!");
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
    setuproutes();
    LOG_INFO("Server starting");
    ThreadPool pool(4);
    LOG_INFO("Creating thread pool created with 4 threads")
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
    while (true)
    {
        // epoll_wait函数等待epoll描述符上注册的文件描述符有IO事件发生，nfds表示实际发生的事件数量，
        // events是用于存储事件信息的缓冲区，MAX_EVENTS定义了缓冲区大小，-1表示无限期等待。
        nfds = epoll_wait(epollfd, events, MAX_EVENT, -1);
        for (int i = 0; i < nfds; i++)
        {
            if (events[i].data.fd == server_fd) // new connection arrive
            {
                // accept connection
                new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addlen);
                setNonBlocking(new_socket);
                // 初始化一个新的epoll事件结构体ev，设置监听新连接的可读事件和边缘触发模式
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
            else
            {
                int client_fd = events[i].data.fd;
                pool.enqueue([client_fd]()
                {
                // read request
                char buffer[1024] = {0};
                read(client_fd, buffer, 1024);
                string request(buffer);
                LOG_INFO("Receiving request\n %s", request.c_str());
                // parse request
                auto [method, uri, body] = parseHttpRequest(request);
                string response_body;
                // handle request
                response_body = handlerequest(method, uri, body);
                LOG_INFO("Sending back: %s", response_body.c_str());
                // send back
                string response = "HTTP/1.1 200 OK\nContent-Type: text/plain\n\n" + response_body;
                send(client_fd, response.c_str(), response.size(), 0);
                close(client_fd);
                });
                LOG_INFO("Task added to thread pool for socket: %s" + events[i].data.fd);
            }
        }
    }
    return 0;
}
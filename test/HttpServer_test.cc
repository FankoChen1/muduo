#include "TcpServer.h"
#include "Logger.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include <iostream>

class HttpServer {
public:
    HttpServer(EventLoop* loop, const InetAddress& addr, const std::string& name)
        : server_(loop, addr, name) {
        server_.setConnectionCallback(
            [](const TcpConnectionPtr& conn) {
                if (conn->connected()) {
                    LOG_INFO("Client UP: %s\n", conn->peerAddress().toIpPort().c_str());
                } else {
                    LOG_INFO("Client DOWN: %s\n", conn->peerAddress().toIpPort().c_str());
                }
            });
        server_.setMessageCallback(
            [this](const TcpConnectionPtr& conn, Buffer* buf, Timestamp) { onMessage(conn, buf); });
    }

    void start() { server_.start(); }

private:
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf) {
        std::string req = buf->retrieveAllAsString();
        // 简单判断是否为 HTTP 请求
        if (req.find("GET") == 0 || req.find("POST") == 0) {
            std::string response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 12\r\n"
                "Connection: close\r\n"
                "\r\n"
                "Hello World\n";
            conn->send(response);
            conn->shutdown(); // HTTP短连接，发送完就关闭
        }
    }

    TcpServer server_;
};

int main() {
    EventLoop loop;
    InetAddress addr(8888);
    HttpServer server(&loop, addr, "HttpServer");
    server.start();
    loop.loop();
    return 0;
}
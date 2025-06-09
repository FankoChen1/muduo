#include "TcpServer.h"
#include "Logger.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include <iostream>
#include <thread>
#include <atomic>

class ChatServer {
public:
    ChatServer(EventLoop* loop, const InetAddress& addr, const std::string& name)
        : server_(loop, addr, name)
        , loop_(loop) 
    {
        server_.setConnectionCallback(
            [this](const TcpConnectionPtr& conn) { onConnection(conn); });
        server_.setMessageCallback(
            [this](const TcpConnectionPtr& conn, Buffer* buf, Timestamp time) { onMessage(conn, buf, time); });
    }

    void start() { server_.start(); }

private:
    void onConnection(const TcpConnectionPtr& conn) {
        if (conn->connected()) {
            LOG_INFO("Client UP: %s\n", conn->peerAddress().toIpPort().c_str());
            conn_ = conn;
            // 启动输入线程，只启动一次
            if (!inputThreadStarted_.exchange(true)) {
                std::thread([this] {
                    std::string line;
                    while (std::getline(std::cin, line)) {
                        TcpConnectionPtr conn = conn_; // 拷贝智能指针，防止析构
                        if (conn && conn->connected()) {
                            conn->send(line + "\n", sizeof(line)+2);
                        }
                    }
                }).detach();
            }
        } else {
            LOG_INFO("Client DOWN: %s\n", conn->peerAddress().toIpPort().c_str());
            conn->setMessageCallback(MessageCallback());
            conn_.reset();
        }
    }

    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp) {
        std::string msg = buf->retrieveAllAsString();
        std::cout << "Client: " << msg;
    }

    TcpServer server_;
    EventLoop* loop_;
    TcpConnectionPtr conn_;
    std::atomic_bool inputThreadStarted_{false};
};

int main() {
    EventLoop loop;
    InetAddress addr(8888);
    ChatServer server(&loop, addr, "ChatServer");
    server.start();
    loop.loop();
    return 0;
}
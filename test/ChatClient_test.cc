#include "TcpConnection.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "Logger.h"
#include <iostream>
#include <thread>
#include <memory>
#include <atomic>

class ChatClient : public std::enable_shared_from_this<ChatClient> {
public:
    ChatClient(EventLoop* loop, const InetAddress& serverAddr)
        : loop_(loop), serverAddr_(serverAddr), conn_(nullptr) {}

    void start() {
        int sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            LOG_ERROR("socket error\n");
            return;
        }
        if (::connect(sockfd, reinterpret_cast<const sockaddr*>(serverAddr_.getSockAddr()), sizeof(sockaddr_in)) < 0) {
            LOG_ERROR("connect error\n");
            ::close(sockfd);
            return;
        }
        conn_ = std::make_shared<TcpConnection>(loop_, "ChatClientConn", sockfd, InetAddress(), serverAddr_);
        conn_->setConnectionCallback([this](const TcpConnectionPtr& conn) { onConnection(conn); });
        conn_->setMessageCallback([this](const TcpConnectionPtr& conn, Buffer* buf, Timestamp time) { onMessage(conn, buf, time); });
        conn_->connectEstablished();
    }

private:
    void onConnection(const TcpConnectionPtr& conn) {
        if (conn->connected()) {
            LOG_INFO("Connected to server.\n");
            // 启动输入线程，只启动一次
            if (!inputThreadStarted_.exchange(true)) {
                std::thread([this] {
                    std::string line;
                    while (std::getline(std::cin, line)) {
                        TcpConnectionPtr conn = conn_; // 拷贝智能指针，防止析构
                        if (conn && conn->connected()) {
                            conn->send(line + "\n");
                        }
                    }
                }).detach();
            }
        } else {
            LOG_INFO("Disconnected from server.\n");
            conn_.reset();
        }
    }

    void onMessage(const TcpConnectionPtr&, Buffer* buf, Timestamp) {
        if (!conn_ || !conn_->connected()) return;
        std::string msg = buf->retrieveAllAsString();
        std::cout << "Server: " << msg;
    }

    EventLoop* loop_;
    InetAddress serverAddr_;
    TcpConnectionPtr conn_;
    std::atomic_bool inputThreadStarted_{false};
};

int main() {
    EventLoop loop;
    InetAddress serverAddr(8888, "127.0.0.1");
    auto client = std::make_shared<ChatClient>(&loop, serverAddr);
    client->start();
    loop.loop();
    return 0;
}
#pragma once

/*
使用muduo编写服务器程序
*/

#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>

#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Buffer.h"

// 使用时设置好callback，再调用start()即可
class TcpServer : noncopyable
{
public:
    // 用于线程池中每个 EventLoop 初始化时调用
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    enum Option
    {
        kNoReusePort,   // 不允许重用本地端口
        kReusePort      // 允许重用本地端口
    };

    TcpServer(EventLoop *loop,
              const InetAddress &listenAddr,
              const std::string &nameArg,
              Option option = kNoReusePort);
    ~TcpServer();

    // 设置线程初始化回调
    void setThreadInitCallback(const ThreadInitCallback &cb) { threadInitCallback_ = cb; }
    // 设置新连接建立/关闭时的回调
    void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
    // 设置消息到达时回调
    void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
    // 设置消息发送完成时的回调
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }

    void setThreadNum(int numThreads);
    void start();

    const std::string name() { return name_; }

    const std::string ipPort() { return ipPort_; }

    EventLoop * getLoop() const { return loop_; }
private:
    // 新连接到来时的回调
    void newConnection(int sockfd, const InetAddress &peerAddr);
    // 移除连接的端口
    void removeConnection(const TcpConnectionPtr &conn);
    // 实际执行在所属的EvnetLoop线程中移除连接的回调，由removeConnection传入runInLoop
    void removeConnectionInLoop(const TcpConnectionPtr &conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>; // 存储所有TcpConnection和对应名字
    
    EventLoop * loop_;      // main loop(baseloop)

    const std::string ipPort_;
    const std::string name_;    // 服务器名字

    std::unique_ptr<Acceptor> acceptor_;    // 运行在main loop，监听新连接事件

    std::shared_ptr<EventLoopThreadPool> threadPool_;   // 线程池，每个线程一个loop

    ThreadInitCallback threadInitCallback_; // loop线程初始化的回调
    ConnectionCallback connectionCallback_; // 新连接建立或关闭时的回调
    MessageCallback messageCallback_;       // 有读写事件发生时的回调
    WriteCompleteCallback writeCompleteCallback_; // 消息发送完成后的回调

    int numThreads_;    // 线程池中线程的数量
    std::atomic_int started_;   // 服务器启动的次数，用于安全启动
    int nextConnId_;    // 标识接收到的对端的TcpConnection，自增，用于给新连接命名
    ConnectionMap connections_; // 保存所有的连接
};
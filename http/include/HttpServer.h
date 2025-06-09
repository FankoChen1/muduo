#pragma once

#include <string>

#include "TcpServer.h"
#include "EventLoopThreadPool.h"

/*
* 负责监听端口、接收连接、分发请求、调用用户回调处理业务。
*/

class HttpRequest;
class HttpResponse;

class HttpServer : noncopyable
{
public:
    using HttpCallback = std::function<void(const HttpRequest &, HttpResponse *)>;

    HttpServer(EventLoop *loop,
               const InetAddress &listenAddr,
               const std::string &name,
               TcpServer::Option option = TcpServer::kNoReusePort);

    EventLoop *getLoop() const { return server_.getLoop(); }

    /// Not thread safe, callback be registered before calling start().
    void setHttpCallback(const HttpCallback &cb)
    {
        httpCallback_ = cb;
    }

    void setThreadNum(int numThreads)
    {
        server_.setThreadNum(numThreads);
    }

    void start();

private:
    // 处理新 TCP 连接建立或关闭的回调。
    void onConnection(const TcpConnectionPtr &conn);
    // 处理收到 TCP 消息（数据）的回调，解析 HTTP 请求
    void onMessage(const TcpConnectionPtr &conn,
                   Buffer *buf,
                   Timestamp receiveTime);
    // 处理解析后的 HTTP 请求，调用用户设置的回调
    void onRequest(const TcpConnectionPtr &, const HttpRequest &);

    TcpServer server_;
    // http请求处理回调函数，在HttpServer类的onMessage函数中被调用
    HttpCallback httpCallback_;

    std::unique_ptr<EventLoopThreadPool> threadPool_;
};
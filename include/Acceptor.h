#pragma once

#include <functional>

#include "noncopyable.h"
#include "Socket.h"
#include "Channel.h"

class EventLoop;
class InetAddress;

class Acceptor : noncopyable
{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress &)>;

    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();

    // 设置新连接的回调函数
    void setNewConnectionCallback(const NewConnectionCallback &cb) { NewConnectionCallback_ = cb; }
    // 判断是否在监听
    bool listening() const { return listening_; }
    // 监听本地端口
    void listen();
private:
    // 处理新用户的连接事件
    void handleRead();

    EventLoop *loop_;   // main loop(baseLoop)
    Socket acceptSocket_;    // 专门用于接受新连接的socket，server socket
    Channel acceptChannel_; // 专门用于监听新连接acceptSocket_的channel
    NewConnectionCallback NewConnectionCallback_;   // 新连接的回调函数
    bool listening_;   // 标识是否在监听
};
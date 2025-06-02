#pragma once

#include "noncopyable.h"

class InetAddress;

// 封装socket fd
class Socket : noncopyable
{
public:
    explicit Socket(int sockfd)
        : sockfd_(sockfd)
    {
    }
    ~Socket();

    int fd() const { return sockfd_; }
    void bindAddress(const InetAddress &localaddr);
    void listen();
    int accept(InetAddress *peeraddr);
    // 关闭服务器的写端
    void shutdownWrite();
    // 设置是否禁用Nagle算法
    void setTcpNoDelay(bool on);
    // 是否允许端口复用
    void setReuseAddr(bool on);
    // 是否允许多个socket绑定到同一端口
    void setReusePort(bool on);
    // 检测长连接是否断开
    void setKeepAlive(bool on);
    // 添加长连接心跳检测自定义时间功能，idle：连接空闲多少秒后开启；interval：两次检测间隔多少秒；count：断开前重试次数
    void setKeepAlive(int idle, int interval, int count);
    
private:
    const int sockfd_;
};
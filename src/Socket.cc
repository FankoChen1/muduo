#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include "Socket.h"
#include "Logger.h"
#include "InetAddress.h"

Socket::~Socket()
{
    ::close(sockfd_);
}

void Socket::bindAddress(const InetAddress &localaddr)
{
    if( 0 != ::bind(sockfd_, (sockaddr *)localaddr.getSockAddr(), sizeof(sockaddr_in)))
    {
        LOG_FATAL("%s:%s:%d bind sockfd:%d fail\n", __FILE__, __FUNCTION__, __LINE__, sockfd_);
    }
}

void Socket::listen()
{
    if(0 != ::listen(sockfd_, 1024))
    {
        LOG_FATAL("%s:%s:%d listen sockfd:%d fail\n", __FILE__, __FUNCTION__, __LINE__, sockfd_);
    }
}

int Socket::accept(InetAddress *peeraddr)
{
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    ::memset(&addr, 0, len);
    int connfd = ::accept4(sockfd_, (sockaddr *)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if(connfd >= 0)
    {
        peeraddr->setSockAddr(addr);
    }
    return connfd;
}

void Socket::shutdownWrite()
{
    if(::shutdown(sockfd_, SHUT_WR) < 0)
    {
        LOG_ERROR("%s:%s:%d shutdownWrite error.\n",  __FILE__, __FUNCTION__, __LINE__);
    }
}

void Socket::setTcpNoDelay(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}

void Socket::setReuseAddr(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

void Socket::setReusePort(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
}

void Socket::setKeepAlive(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}

void Socket::setKeepAlive(int idle, int interval, int count)
{
    int optval = 1;  // 默认开启心跳检测
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
    
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));
}
#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>

//InetAddress 是对 sockaddr_in 的面向对象封装，提供了构造、格式化、访问和设置等功能
class InetAddress
{
public:
    explicit InetAddress(uint16_t port = 8080, std::string ip = "127.0.0.1");
    explicit InetAddress(const sockaddr_in &addr)
        : addr_(addr)
    {
    }

    std::string toIp() const;
    std::string toIpPort() const;
    uint16_t toPort() const;

    const sockaddr_in *getSockAddr() const { return &addr_; }
    void setSockAddr(const sockaddr_in &addr) { addr_ = addr; }

private:
    sockaddr_in addr_;
};
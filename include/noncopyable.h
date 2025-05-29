#pragma once

// 单例模式的基类，禁止派生类拷贝构造和拷贝复制
class noncopyable
{
public:
    noncopyable(const noncopyable &) = delete;
    noncopyable &operator = (const noncopyable &) = delete;
protected:
    noncopyable() = default;
    ~noncopyable() = default;
};
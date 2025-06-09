#pragma once

#include <hiredis/hiredis.h>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <string>
#include <atomic>

class RedisConnectionPool {
public:
    // 获取连接池实例 (单例模式)
    static RedisConnectionPool& getInstance();
    
    // 初始化连接池
    void init(const std::string& host, int port, 
              const std::string& password = "", 
              int db = 0, 
              int poolSize = 10);
    
    // 获取一个连接
    std::shared_ptr<redisContext> getConnection();
    
    // 释放连接回池中
    void releaseConnection(std::shared_ptr<redisContext> conn);
    
    // 关闭所有连接
    void shutdown();
    
    // 禁止拷贝和赋值
    RedisConnectionPool(const RedisConnectionPool&) = delete;
    RedisConnectionPool& operator=(const RedisConnectionPool&) = delete;

private:
    RedisConnectionPool() = default;
    ~RedisConnectionPool();
    
    // 创建新连接
    std::shared_ptr<redisContext> createConnection();

private:
    std::queue<std::shared_ptr<redisContext>> connections_;
    std::mutex mutex_;
    std::condition_variable condition_;
    
    std::string host_;
    int port_;
    std::string password_;
    int db_;
    int poolSize_;
    
    std::atomic<int> currentSize_{0};
    bool initialized_{false};
};
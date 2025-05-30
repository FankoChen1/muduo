#pragma once

#include <functional>
#include <mutex>
#include <condition_variable>
#include <string>

#include "noncopyable.h"
#include "Thread.h"

class EventLoop;

/*
在新线程中安全地创建和管理一个 EventLoop，并通过条件变量和互斥锁与主线程同步，支持线程初始化回调。
*/
class EventLoopThread : noncopyable
{
public:
    // 线程初始化回调，在创建新线程的EventLoop后进行初始化操作
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    // 接受初始化回调操作和线程名字
    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
                    const std::string &name = std::string());
    ~EventLoopThread();

    // 启动新线程并创建EventLoop，返回新线程中的EventLoop指针
    EventLoop *startLoop();

private:
    // 线程主函数，负责在新线程中创建EventLoop并执行事件循环
    void threadFunc();

    EventLoop *loop_;   // 新线程创建的EventLoop指针
    bool exiting_;      // 标记线程是否正在退出
    Thread thread_;     // 线程对象
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;   // 线程初始化回调函数
};
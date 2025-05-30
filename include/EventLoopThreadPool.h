#pragma once

#include <functional>
#include <string>
#include <vector>
#include <memory>

#include "noncopyable.h"
class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : noncopyable
{
public:
    // 线程初始化回调，在创建新线程的EventLoop后进行初始化操作
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg);
    ~EventLoopThreadPool();

    // 设置线程数量
    void setThreadNum(int numThreads) { numThreads_ = numThreads; }

    // 开启线程池
    void start(const ThreadInitCallback &cb = ThreadInitCallback());

    // 获取新连接到来时选择的EventLoop
    EventLoop *getNextLoop();

    // 获取线程池中全部的EventLoop
    std::vector<EventLoop *> getAllLoops();

    // 返回线程池是否开启
    bool started() const { return started_; }
    // 返回线程池的名字
    const std::string name() const { return name_; }
private:
    EventLoop *baseLoop_;    // 本线程池的EventLoop(main loop)，由主线程在外部创建并传入
    std::string name_;      // 线程池名称
    bool started_;          // 标志线程池是否启动
    int numThreads_;        // 线程的数量
    int next_;              // 新连接到来时所选择的EventLoop索引
    std::vector<std::unique_ptr<EventLoopThread>> threads_; // IO线程列表
    std::vector<EventLoop *> loops_;    // 线程池中的EventLoop列表，由EVentLoopThread创建
};
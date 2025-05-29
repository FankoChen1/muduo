#pragma once

#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>

#include "noncopyable.h"
#include "CurrentThread.h"
#include "TimerQueue.h"

class Channel;
class Poller;

class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    void loop();

    void quit();

    Timestamp pollReturnTime() const { return pollReturnTime_; }

    void runInLoop(Functor cb);

    void queueInLoop(Functor cb);

    TimerId runAt(const Timestamp& time, const Timer::TimerCallback& cb);

    TimerId runAfter(double delay, const Timer::TimerCallback& cb);

    TimerId runEvery(double interval, const Timer::TimerCallback& cb);

    void cancel(TimerId timerId);

    void wakeup();

    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);

    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

private:
    void handleRead();
    void doPendingFunctors();

    using ChannelList = std::vector<Channel *>;

    std::atomic_bool looping_;
    std::atomic_bool quit_;

    const pid_t threadId_;

    Timestamp pollReturnTime_;
    std::unique_ptr<Poller> poller_;
    std::unique_ptr<TimerQueue> timerQueue_;

    int wakeupFd_;
    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_;

    std::atomic_bool callingPendingFunctors_;
    std::vector<Functor> pendingFunctors_;
    std::mutex mutex_;
};
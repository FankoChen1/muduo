#pragma once

#include <vector>
#include <set>

#include "Channel.h"
#include "Timestamp.h"
#include "Callbacks.h"

class EventLoop;
class Timer;
class TimerId;

class TimerQueue : noncopyable
{
public:
    TimerQueue(EventLoop* loop);
    ~TimerQueue();

    // 创建Timer并投递到事件循环线程
    TimerId addTimer(const TimerCallback& cb, Timestamp when, double interval);

    void cancel(TimerId timerId);

private:
    using Entry = std::pair<Timestamp, std::unique_ptr<Timer>>; // 表示一个定时器及其到期时间。

    struct EntryCmp {
        bool operator()(const TimerQueue::Entry& a, const TimerQueue::Entry& b) const {
            if (a.first < b.first) return true;
            if (b.first < a.first) return false;
            return a.second.get() < b.second.get();
        }
    };
    using TimerList = std::set<Entry, EntryCmp>;  // 按到期时间排序的定时器集合。
    using ActiveTimer = std::pair<Timer*, int64_t>; // 唯一标识一个活动定时器。
    using ActiveTimerSet = std::set<ActiveTimer>;   // 活动定时器集合。

    void addTimerInLoop(std::unique_ptr<Timer> timer);
    void cancelInLoop(TimerId timerId);

    void handleRead();
    std::vector<Entry> getExpired(Timestamp now);
    void reset(std::vector<Entry>& expired, Timestamp now);
    // 将定时器插入timers_和activeTimers_队列
    bool insert(std::unique_ptr<Timer> timer);
    
    EventLoop* loop_;
    const int timerfd_;     // 定时器文件描述符，用于定时器到期的事件通知。
    Channel timerfdChannel_;    // 负责监听 timerfd_ 上的事件，并回调 handleRead。
    TimerList timers_;      // 存储按到期时间排序的定时器集合（set）

    ActiveTimerSet activeTimers_;   // 当前活动定时器集合，便于查找和取消。
    bool callingExpiredTimers_;     // 标记当前是否正在调用已到期定时器的回调，原子操作防止并发问题。
    ActiveTimerSet cancelingTimers_;    // 存储在回调过程中被取消的定时器，确保安全地处理取消逻辑。
};
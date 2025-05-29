#pragma once

#include <atomic>
#include <functional>

#include "noncopyable.h"
#include "Timestamp.h"

class Timer : noncopyable
{
public:
    using TimerCallback = std::function<void()>;

    Timer(TimerCallback cb, Timestamp when, double interval)
    : callback_(std::move(cb)),
        expiration_(when),
        interval_(interval),
        repeat_(interval > 0.0),
        sequence_(++s_numCreated_)
    { }

    void run() const
    {
        callback_();
    }

    Timestamp expiration() const  { return expiration_; }
    bool repeat() const { return repeat_; }
    int64_t sequence() const { return sequence_; }

    void restart(Timestamp now);

    static int64_t numCreated() { return s_numCreated_.load(std::memory_order_relaxed); }

 private:

    const TimerCallback callback_;      // 保存定时器到期时要执行的回调函数。
    Timestamp expiration_;      // 定时器的到期时间
    const double interval_;     // 定时器的周期。如果大于 0，则为周期性定时器，否则为一次性定时器
    const bool repeat_;         // 标记是否周期性
    const int64_t sequence_;    // 唯一标识每个定时器，通过静态原子变量 s_numCreated_ 自增获得

    static std::atomic<int64_t> s_numCreated_;  // 用std::atomic保证多线程下的安全自增。
};
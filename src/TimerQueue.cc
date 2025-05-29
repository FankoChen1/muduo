#include "TimerQueue.h"
#include "EventLoop.h"
#include "Timer.h"
#include "TimerId.h"
#include "Logger.h"

#include <sys/timerfd.h>
#include <unistd.h>
#include <string.h>

int createTimerfd()
{
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0)
    {
        LOG_FATAL("%s : Failed in timerfd_create", __FUNCTION__);
    }
    return timerfd;
}

struct timespec howMuchTimeFromNow(Timestamp when)
{
    int64_t microseconds = when.microSecondsSinceEpoch()
                            - Timestamp::now().microSecondsSinceEpoch();
    if (microseconds < 100)
    {
        microseconds = 100;
    }
    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(microseconds / Timestamp::kMicroSecondsPerSecond);
    ts.tv_nsec = static_cast<long>( (microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);
    return ts;
}

void readTimerfd(int timerfd, Timestamp now)
{
    uint64_t howmany;
    ssize_t n = ::read(timerfd, &howmany, sizeof(howmany));
    LOG_INFO("TimerQueue::handleRead() %lu at %s", howmany, now.toString().c_str());
    if(n != sizeof(howmany))
    {
        LOG_ERROR("TimerQueue::handleRead() reads %ld bytes instead of 8", n);
    }
}

void resetTimerfd(int timerfd, Timestamp expiration)
{
    struct itimerspec newValue;
    struct itimerspec oldValue;
    ::memset(&newValue, 0, sizeof newValue);
    ::memset(&newValue, 0, sizeof oldValue);
    newValue.it_value = howMuchTimeFromNow(expiration);
    int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);
    if (ret)
    {
        LOG_ERROR("%s : timerfd_settime()", __FUNCTION__);
    }
}

TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop)
    , timerfd_(createTimerfd())
    , timerfdChannel_(loop, timerfd_)
    , timers_()
{
    timerfdChannel_.setReadCallback(std::bind(&TimerQueue::handleRead, this));
    timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue()
{
    timerfdChannel_.disableAll();
    timerfdChannel_.remove();
    ::close(timerfd_);
}

TimerId TimerQueue::addTimer(const Timer::TimerCallback& cb, Timestamp when, double interval)
{
    std::unique_ptr<Timer> timer(new Timer(cb, when, interval));
    TimerId id(timer.get(), timer->sequence());
    // C++11 lambda捕获外部变量
    Timer* timerPtr = timer.release();
    loop_->runInLoop(
        [this, timerPtr]() {
            std::unique_ptr<Timer> timer(timerPtr);
            addTimerInLoop(std::move(timer));
        }
    );
    return id;
}

void TimerQueue::cancel(TimerId timerId)
{
    loop_->runInLoop(std::bind(&TimerQueue::cancelInLoop, this, timerId));
}

void TimerQueue::addTimerInLoop(std::unique_ptr<Timer> timer)
{
    if(!loop_->isInLoopThread()) LOG_FATAL("TimerQueue::addTimerInLoop() : the thread is not in loop");
    bool earliestChanged = insert(std::move(timer));
    if(earliestChanged)
    {
        resetTimerfd(timerfd_, timers_.begin()->second->expiration());
    }
}

void TimerQueue::cancelInLoop(TimerId timerId)
{
    if(!loop_->isInLoopThread()) LOG_FATAL("TimerQueue::handleRead() : the thread is not in loop");
    if (timers_.size() != activeTimers_.size())
    {
        LOG_FATAL("TimerQueue::cancelInLoop() before erase: timers_ and activeTimers_'s numbers are different\n");
    }
    ActiveTimer timer(timerId.timer_, timerId.sequence_);
    ActiveTimerSet::iterator it = activeTimers_.find(timer);
    if (it != activeTimers_.end())
    {
        // 删除 timers_ 里的 unique_ptr
        for (auto iter = timers_.begin(); iter != timers_.end(); ++iter) {
            if (iter->second.get() == it->first) {
                timers_.erase(iter);
                break;
            }
        }
        activeTimers_.erase(it);
    }
    else if (callingExpiredTimers_)
    {
        cancelingTimers_.insert(timer);
    }
}

void TimerQueue::handleRead()
{
    if(!loop_->isInLoopThread())
    {
        LOG_FATAL("TimerQueue::handleRead() : the thread is not in loop");
    }
    Timestamp now(Timestamp::now());
    readTimerfd(timerfd_, now);

    std::vector<Entry> expired = getExpired(now);

    callingExpiredTimers_ = true;
    cancelingTimers_.clear();

    for(const Entry& it : expired)
    {
        it.second->run();
    }
    callingExpiredTimers_ = false;

    reset(expired, now);
}

std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
{
    if (timers_.size() != activeTimers_.size())
    {
        LOG_FATAL("TimerQueue::getExpired() before erase: timers_ and activeTimers_'s numbers are different\n");
    }
    std::vector<Entry> expired;
    // 构造一个“哨兵”Entry，主要用当前时间 now 作为键，值为一个最大指针（只用于比较，不会被实际用到）。
    Entry sentry(now, nullptr);
    TimerList::iterator end = timers_.lower_bound(sentry);
    for (auto it = timers_.begin(); it != end; )
    {
        expired.push_back(std::make_pair(it->first, std::move(const_cast<std::unique_ptr<Timer>&>(it->second))));
        it = timers_.erase(it);
    }

    for (const Entry& it : expired)
    {
        ActiveTimer timer(it.second.get(), it.second->sequence());
        size_t n = activeTimers_.erase(timer);
        if (n != 1)
        {
            LOG_FATAL("TimerQueue::getExpired() : erase an expired Timer failed!\n");
        }
    }

    if (timers_.size() != activeTimers_.size())
    {
        LOG_FATAL("TimerQueue::getExpired() after erase: timers_ and activeTimers_'s numbers are different\n");
    }

    return expired;
}

void TimerQueue::reset(std::vector<Entry>& expired, Timestamp now)
{
    Timestamp nextExpire;

    for (auto& it : expired)
    {
        ActiveTimer timer(it.second.get(), it.second->sequence());
        if (it.second->repeat() && cancelingTimers_.find(timer) == cancelingTimers_.end())
        {
            it.second->restart(now);
            insert(std::move(it.second)); // 转移所有权
        }
        // else 自动析构
    }

    if (!timers_.empty())
    {
        nextExpire = timers_.begin()->second->expiration();
    }

    if (nextExpire.valid())
    {
        resetTimerfd(timerfd_, nextExpire);
    }
}

bool TimerQueue::insert(std::unique_ptr<Timer> timer)
{
    if(!loop_->isInLoopThread())
    {
        LOG_FATAL("TimerQueue::insert() : the thread is not in loop");
    }
    if (timers_.size() != activeTimers_.size())
    {
        LOG_FATAL("TimerQueue::insert() before insert : timers_ and activeTimers_'s numbers are different\n");
    }
    bool earliestChanged = false;
    Timestamp when = timer->expiration();
    TimerList::iterator it = timers_.begin();
    if (it == timers_.end() || when < it->first)
    {
        earliestChanged = true;
    }

    Timer* timerPtr = timer.get();
    auto result = timers_.insert(Entry(when, std::move(timer)));
    if(!result.second) { LOG_FATAL("TimerQueue::insert() : Failed in timers_.insert()\n"); }

    auto result2 = activeTimers_.insert(ActiveTimer(timerPtr, timerPtr->sequence()));
    if(!result2.second) { LOG_FATAL("TimerQueue::insert() : Failed in activeTimers_.insert()\n"); }

    return earliestChanged;
}
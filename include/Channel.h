#pragma once

#include <functional>
#include <memory>

#include "noncopyable.h"
#include "Timestamp.h"

class EventLoop;

class Channel : noncopyable
{
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    // 指定所属的EventLoop和监听的文件描述符
    Channel(EventLoop *loop, int fd);
    ~Channel();

    // handleEvent 处理的是IO事件（如网络、定时器、wakeup等）
    // 对外接口，在安全的情况下调用handleEventWithGuard处理fd上发生的事件
    void handleEvent(Timestamp receiveTime);
    // 设置事件回调
    void setReadCallback(const ReadEventCallback &cb)
    { readCallback_ = std::move(cb); }
    void setWriteCallback(const EventCallback &cb)
    { writeCallback_ = std::move(cb); }
    void setCloseCallback(const EventCallback &cb)
    { closeCallback_ = std::move(cb); }
    void setErrorCallback(const EventCallback &cb)
    { errorCallback_ = std::move(cb); }

    // 绑定一个对象的生命周期，防止回调时对象已销毁
    void tie(const std::shared_ptr<void> &);
    // 获取监听的文件描述符
    int fd() const { return fd_; }
    // 获取当前关注的事件类型
    int events() const { return events_; }
    // 设置实际发生的事件类型（由 poll/epoll 填充）
    void set_revents(int revt) { revents_ = revt; }

    // 修改关注的事件类型，并通知 poller 更新
    void enableReading() { events_ |= kReadEvent; update(); }
    void disableReading() { events_ &= ~kReadEvent; update(); }
    void enableWriting() { events_ |= kWriteEvent; update(); }
    void disableWriting() { events_ &= ~kWriteEvent; update(); }
    void disableAll() { events_ = kNoneEvent; update(); }

    // 查询当前关注的事件类型
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    // 返回poller内部的channel的下标
    int index() { return index_; }
    // 设置poller内部的channel的下标
    void set_index(int idx) { index_ = idx; }

    // 返回所属的EventLoop的指针
    EventLoop *ownerLoop() { return loop_; }
    // 在EventLoop中移除该Channel
    void remove();
private:
    // 通知 poller 更新该 Channel 的事件关注类型
    void update();
    // 真正分发和执行各类事件回调
    void handleEventWithGuard(Timestamp receiveTime);

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop *loop_;   // Channel所属的EventLoop
    const int fd_;      // 该Channel监听的文件描述符
    int events_;        // 当前关注的事件类型
    int revents_;       // 实际发生的事件类型(由Poller填充)
    int index_;         // 该Channel在所属的Poller中的唯一标识

    std::weak_ptr<void> tie_;   // 用于绑定 TcpConnection 等对象的生命周期
    bool tied_;                 // 标记是否已绑定对象

    // 事件回调
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};
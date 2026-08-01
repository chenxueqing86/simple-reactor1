#pragma once

#include <functional>
#include <chrono>
#include <memory>
#include <atomic>
#include <sys/timerfd.h>

class EventLoop;
class Channel;

class Timer{
    public:
    using TimerCallback = std::function<void()>;

    Timer(EventLoop* loop,
         std::chrono::milliseconds interval,
        TimerCallback cb,
         bool repeat);
    ~Timer();

    //启动定时器
    void start();
    //停止定时器
    void stop();
    void run();
    bool isActive() const { return isActive_; }
    // 重置定时器（例如在超时后重新设定）
    void reset();
    int fd() const { return timerfd_;}

    private:
    void handleRead(); //由 Channel 回调

    EventLoop* loop_;
    int timerfd_;
    std::chrono::milliseconds interval_;
    TimerCallback callback_;
    bool repeat_;
    bool isActive_;
    std::unique_ptr<Channel> channel_;
//    std::atomic<bool> isActive_;
   // std::unique_ptr<class Channel> channel_; //前向声明，实际用Channel
};
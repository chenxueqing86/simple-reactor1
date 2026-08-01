#pragma once
#include <vector>
#include <map>
#include <sys/epoll.h>
//day4 newinsert
#include <memory>
#include <functional>
#include <chrono>
#include <mutex>
#include <thread>
#include <atomic>
#include <algorithm>
#include <sys/timerfd.h>

class Channel;

class Timer;

class EventLoop{
public:
    EventLoop();
    ~EventLoop();

    void loop();  //主事件循环
    void quit(); //新增推出方法
    void updateChannel(Channel* ch); //添加或更新Channel
    void removeChannel(Channel* ch); //移除 Channel

    //day4 继承定时器，新增 runAfter 和 runEvery接口
    //定时器接口
    void runAfter(std::chrono::milliseconds delay,std::function<void()> cb);
    void runEvery(std::chrono::milliseconds interval,std::function<void()> cb);
    //取消定时器（需要返回TimerId,此处略）

    // 跨线程任务投递
    void runInLoop(std::function<void()> cb);
    void queueInLoop(std::function<void()> cb);
    void wakeup();
 
    bool isInLoopThread() const { return threadId_ == std::this_thread::get_id(); }

private:
    int epollfd_;
    std::vector<struct epoll_event> events_;
    std::map<int,Channel*> channelMap_; //fd-> Channel*

    void fillActiveChannels(int numEvents);

    //day4
     int wakeupFd_; //evenfd
     int timerfd_;
     std::unique_ptr<Channel> timerChannel_;
    std::unique_ptr<Channel> wakeupChannel_;

    //定时器管理
    struct TimerEntry{
        std::shared_ptr<Timer> timer;
        std::chrono::steady_clock::time_point expiration;
        bool operator>(const TimerEntry& other) const{
            return expiration > other.expiration;
        }
    };
    std::vector<TimerEntry> timerHeap_;
    std::mutex timerMutex_;

    std::vector<Channel*> activeChannels_;

    void handleWakeup();  // wakeup fd 的回调
    void doPendingFunctors();
    void handleTimerRead();
    void resetTimerfd();

   //跨线程任务
    std::vector<std::function<void()>> pendingFunctors_;
    std::mutex mutex_;
    bool callingPendingFunctors_;

    std::thread::id threadId_;
    std::atomic<bool> quit_;
    //定时器管理（直接用 vector 持有 shared_ptr）
    std::vector<std::shared_ptr<Timer>> timers_;
};
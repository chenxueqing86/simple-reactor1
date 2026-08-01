#include "EventLoop.h"
#include "Channel.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <cstring>
#include <cassert>
#include <iostream>
//day4
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include "Timer.h"
#include <algorithm>
//#include "TimerQueue.h"
#include <memory>

EventLoop::EventLoop()
    : epollfd_(epoll_create1(EPOLL_CLOEXEC)),
    wakeupFd_(eventfd(0,EFD_NONBLOCK | EFD_CLOEXEC)),
    timerfd_(timerfd_create(CLOCK_MONOTONIC,TFD_NONBLOCK | TFD_CLOEXEC)),
    //events_(64),
    events_(1024),
    callingPendingFunctors_(false),
    threadId_(std::this_thread::get_id()),
    quit_(false){
     //   assert(epollfd_ >= 0);
      //  assert(wakeupFd_ >= 0);
      //if(epollfd_ < 0 || wakeupFd_ < 0 || timerfd_ < 0)
      if(epollfd_ < 0 || wakeupFd_ < 0 ){
        std::cerr << "EventLoop init failed " << std::endl;
        abort();
      }
    //day4
//    wakeupFd_ = eventfd(0,EFD_NONBLOCK | EFD_CLOEXEC);
    wakeupChannel_ = std::make_unique<Channel>(this,wakeupFd_);
    wakeupChannel_->setReadCallback([this]() { handleWakeup(); });
    wakeupChannel_->enableReading();

   // timerChannel_ = std::make_unique<Channel>(this,timerfd_);
   timerChannel_.reset(new Channel(this,wakeupFd_));
    timerChannel_ ->setReadCallback([this]() { handleTimerRead(); });
    timerChannel_ ->enableReading();
}


EventLoop::~EventLoop(){
    close(epollfd_);
    close(wakeupFd_);
   // close(timerfd_);
}

void EventLoop::updateChannel(Channel* ch){
    struct epoll_event ev;
    ev.events = ch->events();
    ev.data.fd = ch->fd();
    if(channelMap_.find(ch->fd()) == channelMap_.end()){
        epoll_ctl(epollfd_,EPOLL_CTL_ADD,ch->fd(),&ev);
        channelMap_[ch->fd()] = ch;
    }else{
        epoll_ctl(epollfd_,EPOLL_CTL_MOD,ch->fd(),&ev);
    }
}

void EventLoop::removeChannel(Channel* ch){
    int fd = ch->fd();
    auto it = channelMap_.find(fd);
    if(it != channelMap_.end()){
        epoll_ctl(epollfd_,EPOLL_CTL_DEL,fd,nullptr);
        channelMap_.erase(it);
    }
}

void EventLoop::loop(){
    while(!quit_){
        int numEvents = epoll_wait(epollfd_,events_.data(),
                        static_cast<int>(events_.size()),-1);
    if(numEvents < 0){ 
        if(errno == EINTR) continue;
        std::cerr << "epoll_wait error" << std::endl;
        break;
    }
   for(int i = 0;i < numEvents;++i){
    int fd = events_[i].data.fd;
    auto it = channelMap_.find(fd); //需要 channelMap_
    if(it != channelMap_.end()){
        Channel* ch = it->second;
        ch->setRevents(events_[i].events);
        ch->handleEvent();
    }
   }
   doPendingFunctors();
 }
}

void EventLoop::fillActiveChannels(int numEvents){
    for(int i = 0;i < numEvents;++i){
        int fd = events_[i].data.fd;
        auto it = channelMap_.find(fd);
        if(it != channelMap_.end()){
            Channel* ch = it->second;
            ch->setRevents(events_[i].events);
            ch->handleEvent();
        }
    }
}

//day4
void EventLoop::wakeup(){
    uint64_t one = 1;
  //  write(wakeupFd_,&one,sizeof(one));
    ssize_t n = write(wakeupFd_,&one,sizeof(one));
  /*  if(n != sizeof(one)){
        //Log error
        std::cerr << "EventLoop::wakeup() writes " << n << " bytes instead of 8"
        << std::endl;
    } */ 
     (void)n;
}

void EventLoop::handleWakeup(){
    uint64_t one;
   // read(wakeupFd_,&one,sizeof(one));
    ssize_t n = read(wakeupFd_,&one,sizeof(one));
   /* if(n != sizeof(one)){
        std::cerr << "EventLoop::handleWakeup() reads " << n <<
        " bytes instead of 8" << std::endl;
    } */
     (void)n;   //处理 pending functors
    doPendingFunctors();
}

void EventLoop::runInLoop(std::function<void()> cb){
    if(isInLoopThread()){
        cb();
    }else{
        queueInLoop(std::move(cb));
    }
}

void EventLoop::queueInLoop(std::function<void()> cb){
   {
     std::lock_guard<std::mutex> lock(mutex_);
    pendingFunctors_.push_back(std::move(cb));
   }
   if(!isInLoopThread() || callingPendingFunctors_){
    wakeup();
   }
}

void EventLoop::doPendingFunctors(){
    std::vector<std::function<void()>> functors; //可调用对象包装器，位于头文件 <functional>
    {
        std::lock_guard<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    callingPendingFunctors_ = true;
    for(auto& f : functors){
        f();
    }
    callingPendingFunctors_ = false;
}

//--------- 定时器实现 ----------
void EventLoop::runAfter(std::chrono::milliseconds delay,std::function<void()> cb){
  //  timerQueue_->addTimer(delay,std::move(cb),false);
  auto timer = std::make_shared<Timer>(this,delay,std::move(cb),false);
  {
    std::lock_guard<std::mutex> lock(timerMutex_);
    timers_.push_back(timer);
  }
  timer->start(); //启动 timefd
 /* auto now = std::chrono::steady_clock::now();
  auto expiration = now + delay;
  {
    std::lock_guard<std::mutex> lock(timerMutex_);
    timerHeap_.push_back({timer,expiration});
    std::push_heap(timerHeap_.begin(),timerHeap_.end(),
          [](const TimerEntry& a,const TimerEntry& b){
            return a.expiration > b.expiration;
          });
  }
  resetTimerfd();*/
}

void EventLoop::runEvery(std::chrono::milliseconds interval,std::function<void()> cb)
{
  // timerQueue_->addTimer(interval,std::move(cb),true);
  auto timer = std::make_shared<Timer>(this,interval,std::move(cb),true);
  {
    std::lock_guard<std::mutex> lock(timerMutex_);
    timers_.push_back(timer);
  } 
  timer->start();
  //如果需要管理，可以保存到列表，此处简略
}

void EventLoop::resetTimerfd(){
    if(timerHeap_.empty()){
        struct itimerspec ts;
        memset(&ts,0,sizeof(ts));
        timerfd_settime(timerfd_,0,&ts,nullptr);
        return;
    }
    auto exp = timerHeap_.front().expiration;
    auto now = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(exp - now);
    if(ms.count() < 0) ms = std::chrono::milliseconds(0);
    struct itimerspec ts;
    memset(&ts,0,sizeof(ts));
    ts.it_value.tv_sec = ms.count()/1000;
    ts.it_value.tv_nsec = (ms.count() % 1000)* 1000000;
    timerfd_settime(timerfd_,0,&ts,nullptr);
}

void EventLoop::handleTimerRead(){
    uint64_t exp;
    ssize_t n = read(timerfd_,&exp,sizeof(exp));
    if(n != sizeof(exp)) return;
    auto now = std::chrono::steady_clock::now();
    std::vector<std::shared_ptr<Timer>> expriedTimers;
    {
        std::lock_guard<std::mutex> lock(timerMutex_);
        while(!timerHeap_.empty() && timerHeap_.front().expiration <= now){
            expriedTimers.push_back(timerHeap_.front().timer);
            std::pop_heap(timerHeap_.begin(),timerHeap_.end(),
                  [](const TimerEntry& a,const TimerEntry& b){
                    return a.expiration > b.expiration;
                  });
                  timerHeap_.pop_back();
        } 
    }
    for(auto& timer : expriedTimers){
        timer->run(); // 执行回调，单词定时器会自动停止
    }
    resetTimerfd();
}


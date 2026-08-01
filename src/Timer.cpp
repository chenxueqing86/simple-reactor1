#include "Timer.h"
#include "EventLoop.h"
#include "Channel.h"
#include <unistd.h>
#include <sys/socket.h>
#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <sys/timerfd.h>
//#define _GNU_SOURCE

Timer::Timer(EventLoop* loop,std::chrono::milliseconds interval,
             TimerCallback cb,bool repeat)
      :loop_(loop),interval_(interval),callback_(std::move(cb)),
      repeat_(repeat),isActive_(false){
        timerfd_ = timerfd_create(CLOCK_MONOTONIC,TFD_NONBLOCK | TFD_CLOEXEC);
        if(timerfd_ == -1){
            perror("timerfd_create");
            abort();
        }
        channel_ = std::make_unique<Channel>(loop_,timerfd_);
        channel_->setReadCallback([this]() { handleRead(); });
      }

      Timer::~Timer(){
        stop();
        close(timerfd_);
      }

      void Timer::start(){
        if(isActive_) return;
        struct itimerspec ts;
        memset(&ts,0,sizeof(ts));
        ts.it_value.tv_sec = interval_.count()/1000;
        ts.it_value.tv_nsec = (interval_.count() % 1000)* 1000000;
        if(repeat_){
            ts.it_interval = ts.it_value;
        }
        if(timerfd_settime(timerfd_,0,&ts,nullptr) == -1){
            perror("timerfd_settime");
            return;
        }
        isActive_ = true;
        channel_->enableReading();
      }

      void Timer::stop(){
        if(!isActive_) return;
        struct itimerspec ts;
        memset(&ts,0,sizeof(ts));
        timerfd_settime(timerfd_,0,&ts,nullptr);
        isActive_ = false;
        channel_->disableAll();
      }

      void Timer::run(){
        if(callback_) callback_();
        if(!repeat_){
          stop();
        }
      }
     /* void Timer::reset(){
        stop();
        start();
      }*/

      void Timer::handleRead(){
        uint64_t exp;
        ssize_t n = read(timerfd_,&exp,sizeof(exp));
        if(n != sizeof(exp)){
            //错误处理
            return;
        }
        run();
        /*if(callback_) callback_();
        if(!repeat_){
            stop(); //单次定时器自动停止
        }*/
      }
      
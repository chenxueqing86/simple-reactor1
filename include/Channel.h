#pragma once
#include <functional>
#include <sys/epoll.h>

class EventLoop; //前向声明

class Channel {
public:
       using EventCallback = std::function<void()>;

       Channel(EventLoop* loop,int fd);
       ~Channel();

       //设置回调
       void setReadCallback(EventCallback cb) { readCallback_ = std::move(cb);}
       void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb);}
       void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

       //启用/禁用事件
       void enableReading() { events_ |= EPOLLIN; update(); }
       void enableWriting() { events_ |= EPOLLOUT; update();}
       void disableAll() { events_ = 0;update();}

       //获取信息
       int fd() const { return fd_; }
       int events() const { return events_; }
       void setRevents(int rev) { revents_ = rev;}

       //事件处理入口
       void handleEvent();

private:
     void update(); //调用 Eventloop 更新 epoll

     EventLoop* loop_;
     int fd_;
     int events_; //注册的事件
     int revents_; // 实际发生的事件

     EventCallback readCallback_;
     EventCallback writeCallback_;
     EventCallback errorCallback_;
    };

    
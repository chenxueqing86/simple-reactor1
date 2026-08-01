#include "Channel.h"
#include "EventLoop.h"
#include <unistd.h>
#include <cassert>



Channel::Channel(EventLoop* loop,int fd)
        : loop_(loop),fd_(fd),events_(0),revents_(0) {}

Channel::~Channel(){
    // 析构时从 epoll 中移除（但不关闭fd)
    if(loop_) loop_->removeChannel(this);
}

void Channel::update(){
    loop_ ->updateChannel(this);
}

void Channel::handleEvent(){
    if(revents_ & (EPOLLERR | EPOLLHUP)){
        if(errorCallback_) errorCallback_();
        return;
    }
    if(revents_ & (EPOLLIN | EPOLLPRI)){
        if(readCallback_) readCallback_();
    }

    if(revents_ & EPOLLOUT){
        if(writeCallback_) writeCallback_();
    }
}
#include "Buffer.h"
#include <cstring>
#include <sys/uio.h>
#include <cerrno>
#include <unistd.h>

Buffer::Buffer(size_t initialSize)
    :buffer_(initialSize),readIndex_(0),writeIndex_(0){}

void Buffer::ensureWritable(size_t len){
    //如果已有可用空间足够，直接返回
    if(writableBytes() >= len) return;

    //尝试回收读过的空间（移动可读数据到开头）
    if(readIndex_ > 0){
        size_t readable = readableBytes();
        std::move(buffer_.begin() + readIndex_,
                  buffer_.begin() + writeIndex_,
                  buffer_.begin());
      readIndex_ = 0;
      writeIndex_ = readable;
      if(writableBytes() >= len) return;
    }

    //扩容：至少翻倍
    size_t newSize = buffer_.size();
    while(newSize < writeIndex_ + len) newSize *= 2;
    buffer_.resize(newSize);
}

void Buffer::append(const char* data,size_t len){
    ensureWritable(len);
    std::copy(data,data + len,buffer_.begin() + writeIndex_);
    writeIndex_ += len;
}

void Buffer::append(const std::string& str){
    append(str.c_str(),str.size());
}

void Buffer::retrieve(size_t len){
    if(len > readableBytes())
          throw std::out_of_range("Buffer::retrieve");
    readIndex_ += len;
    if(readIndex_ == writeIndex_) retrieveAll();
}

void Buffer::retrieveAll() {
    readIndex_ = 0;
    writeIndex_ = 0;
}

std::string Buffer::retrieveAsString(size_t len){
    if(len > readableBytes())
        throw std::out_of_range("Buffer::retrieveAsString");
    std::string result(peek(),len);
    retrieve(len);
    return result;
}

ssize_t Buffer::readFd(int fd,int* savedErrno)
{
    //栈上临时缓冲区，处理buffer写满溢出数据
    char extrabuf[65536];
    struct iovec vec[2];

    const size_t writable = writableBytes();
   // vec[0].iov_base = beginwrite();
    vec[0].iov_len = writable;
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    const ssize_t n = ::readv(fd,vec,2);
    if( n < 0)
    {
        *savedErrno = errno;
    }
    else if(static_cast <size_t>(n) <= writable)
    {
        //数据全部写入当前buffer
        writeIndex_ += n;
    }
    else
    {
        //一部分写入临时栈缓冲区，追加进buffer
        writeIndex_ = buffer_.size();
        append(extrabuf,n - writable);
    }
    return n;
}

//对应的 writeFd: 把 Buffer 可读数据写入 fd
ssize_t Buffer::writeFd(int fd,int* savedErrno)
{
    //直接读取readIndex 起始的可读数据，写入fd
    ssize_t n = ::write(fd,peek(),readableBytes());
    if( n < 0)
    {
        *savedErrno = errno;
    }
    else if( n > 0)
    {
        //写完n 字节，移动读指针
        retrieve(n);
    }
    return n;
}
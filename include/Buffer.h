#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <stdexcept>

class Buffer{
public:
    explicit Buffer(size_t initialSize = 1024);

    //写入数据（自动扩容）
    void append(const char* data,size_t len);
    void append(const std::string& str);

    //读取数据（移动读指针）
    std::string retrieveAsString(size_t len);
    void retrieve(size_t len);
    void retrieveAll();

    //只读访问
    const char* peek() const { return &buffer_[readIndex_];}
    size_t readableBytes() const { return writeIndex_ - readIndex_;}
    size_t writableBytes() const { return buffer_.size() - writeIndex_;}

    //读写添加（从fd内核到用户缓冲区）
    ssize_t readFd(int fd,int* savedErrno);
    ssize_t writeFd(int fd,int* savedErrno);

private:
    std::vector<char> buffer_;
    size_t readIndex_;
    size_t writeIndex_;

    void ensureWritable(size_t len);
};


cmake_minimum_required(VERSION 3.10)
project(ReactorNet VERSION 1.0.0)

#//指定C++17,也可以C++14

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#头文件目录
include_directories(${CMAKE_SOURCE_DIR}/include)

#源文件列表（库）
set(SOURCES
    src/Buffer.cpp 
    src/Channel.cpp 
    src/EventLoop.cpp
    src/TcpConnection.cpp 
    src/Timer.cpp 
)

#生成静态库（可选）
add_library(reactor_net STATIC ${SOURCES})

#生成可执行示例
#add_executable(${REACTOR_LEARNING} examples/echo_http_server.cpp )
#target_link_libraries(${REACTOR_LEARNING}  reactor_net pthread)
add_executable(echo_http_server examples/echo_http_server.cpp)
target_link_libraries(echo_http_server reactor_net pthread)

# 支持 Debug/Release 切换
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
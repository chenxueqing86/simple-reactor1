## 1.整体架构
本项目采用 **单线程 Reactor** 模式，核心组件包括：
- `EventLoop`:事件循环，封装 epoll，驱动所有事件
-  `Buffer`: 自动扩容的缓冲区（读写指针分开）
- `Channel`: 文件描述符 + 事件回调
- `TcpConnection`:封装 TCP 连接，管理读写缓冲区和协议解析
- `Timer`: 基于 timerfd 的定时器
+--------------------------+
|        EventLoop         | <-----epoll 事件分发-----+
| (epoll + wakeup eventfd) |                          |
| loop / runInLoop / 定时  |                           |
+--------------------------+                          |
         ↓持有                    |                    |
   ———————————————————————————————————————————         |
   |              |                           |        |
   ↓              ↓                           ↓        |
 +-----------+ +----------+       +------------------+
 |  TcpConn  | |  Timer   |       |     Channel      |
 |  缓冲区   | |  timerfd |        |  read/write/回调 |
 +-----------+ +----------+       +------------------+
     ↓
 TcpConn 内部创建 Channel 绑定 socket fd
 所有 fd（socket/timerfd/eventfd）都必须配套一个 Channel，才能被 EventLoop 的 epoll 监听；
EventLoop 不直接操作 fd，只操作 Channel；
Timer 依靠 timerfd + Channel 接入事件循环；
TcpConnection 封装一条 TCP 连接，内置读写缓冲区，绑定 socket Channel；
wakeup eventfd 配套独立 Channel，实现跨线程唤醒 epoll_wait 阻塞。
## 2.事件循环流程图
EventLoop::loop()
|
+---------epoll_wait() 等待事件
|
+-------遍历循环事件
||
| +-- Channel::handleEvent()
||
| +-- readCallback( handleRead)
|||
|| +-- 循环 recv，填充inputBuffer_
|| +-- 解析协议（长度头/HTTP）
|| +-- 调用业务回调
||
| +--writeCallback(handleWrite)
||
| +-- 从 outputBuffer_ 发送send
| +-- 若写完，取消 EPOLLOUT 事件
|
| +-- 处理跨线程业务（如pendingFunctors_）
+-- 处理定时器 （timefd 事件）

## 3.协议处理
- **默认协议**：长度头（4字节网络序 + 数据体），解决粘包/拆包
- **HTTP 模式**：解析 GET/请求，返回固定HTML 的响应

## 4.定时器实现
- 使用 Linux `timefd`,纳入epoll 事件循环。
- 'runAfter(delay,cb)':单次延迟执行
- 'runEvery(interval,cb)':周期性执行。
- 定时器对象由 'std::shared_ptr' 管理，防止提前析构。

## 5. 跨线程任务投递
- 'EventLoop' 持有 `eventfd`,作为唤醒 fd。
- `queueInLoop(cb)`:将任务加入队列，若不在 IO 线程则 'wakeup()'.
- `runInLoop(cb)`:若在IO线程则直接执行，否则入队
- 在`loop` 的每次循环末尾执行 `pendingFunctors_` ,保证任务及时处理

## 6.性能考量
- 单线程 Reactor 适合CPU 密集型业务？不，适合 IO密集型。
- 瓶颈通常出现在（`recv`/`send`）以及内存拷贝
- 优化方向：增大缓冲区、启用TCP_NODELAY（消除小包合并延迟，适合低延迟场景（音视频、实时指令、RPC）；）、减少日志

## 7.后续扩展
- 多线程Reactor（主线程accept,工作线程处理IO）
- 内存池优化 Buffer 分配
- 更多协议


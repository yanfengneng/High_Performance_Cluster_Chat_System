#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <iostream>
#include <functional>
#include <string>

using namespace muduo;
using namespace muduo::net;
using namespace std::placeholders;

/**
 * 业务代码主要会暴露两方面：1.用户的连接和断开；2.用户的可读写事件。
 */
/**
 * 基于 muduo 网络库开发服务器程序：
 * 1. 组合 TcpServer 对象；
 * 2. 创建 EventLoop 事件循环对象的指针；
 * 3. 明确 TcpServer 构造函数需要什么参数，输出 ChatServer 的构造函数；
 * 4. 在当前服务器类的构造函数中，注册处理连接的回调函数和处理读写事件的回调函数；
 * 5. 设置核数的服务端线程数量，moduo 库会自行分配 I/O 线程和 worker 线程；
 */
class ChatServer {
 public:
  ChatServer(EventLoop* loop /* 事件循环，反应堆 Reactor */,
             const InetAddress& listenAddr /* IP + Port */,
             const string& nameArg /* 服务器的名称，给线程绑定名字 */)
      : server_(loop, listenAddr, nameArg), loop_(loop) {
    // 给服务器注册用户连接的创建和断开回调
    // 使用绑定器bind，绑定 this 对象的 OnConnection() 函数，_1 表示占位符
    server_.setConnectionCallback(
        std::bind(&ChatServer::onConnection, this, _1));
    // 给服务器注册用户读写事件回调
    server_.setMessageCallback(
        std::bind(&ChatServer::onMessage, this, _1, _2, _3));
    // 设置服务器端的线程数量：一个 I/O 线程，三个 worker 线程
    server_.setThreadNum(4);
  }

  ~ChatServer() {}
  /* 开启事件循环 */
  void start(){
    server_.start();
  }
 private:
  /* 业务开发最主要的精力在 onConnection() 和 onMessage() 这两个函数上面 */
  /* 专门处理用户的连接创建和断开：epoll、listenfd、accept */
  void onConnection(const TcpConnectionPtr& conn) {
    if (conn->connected()) {
      std::cout << conn->peerAddress().toIpPort() << "->"
                << conn->localAddress().toIpPort() << " state : online"
                << std::endl;
    } else {
      std::cout << conn->peerAddress().toIpPort() << "->"
                << conn->localAddress().toIpPort() << " state : offline"
                << std::endl;
      conn->shutdown(); // close(fd)
      // loop_->quit();
    }
  }

  /* 专门处理用户的读写事件 */
  void onMessage(const TcpConnectionPtr& conn /* 连接 */,
                 Buffer* buffer /* 缓冲区 */,
                 Timestamp time /* 接收数据的时间信息 */) {
    // 保存所接收到的所有数据
    std::string buf = buffer->retrieveAllAsString();
    // 解析
    std::cout << "recv data: " << buf << " time: " << time.toString() << std::endl;
    // 返回信息；做一个回声定位器
    conn->send(buf);
  }
  TcpServer server_;
  EventLoop* loop_;  // epoll
};

int main()
{
  EventLoop loop; // 相当于创建了 epoll
  InetAddress addr("127.0.0.1", 6000);
  ChatServer server(&loop, addr, "ChatServer");
  // 启动服务：listenfd、epoll_ctl 添加到 epoll 上面
  server.start();
  // epoll_wait 以阻塞的方式等待新用户连接，已连接用户的读写事件等
  loop.loop();
  return 0;
}
/*
g++ [选项] 文件名 -o 输出文件名
-l <library>：链接指定库，注意这里是 l，不是字母i(I)
同时注意：链接库文件一定要放在muduo_server.cpp后面，不然就会编译报错！！！
注意这里面的链接库的顺序也不要弄错了，因为前一个库依赖于后一个库，不然的话就会编译报错了。
编译命令：g++ -g muduo_server.cpp -lmuduo_net -lmuduo_base -lpthread -std=c++11 -o server
g++ muduo_server.cpp -lmuduo_net -lmuduo_base -lpthread -std=c++11
*/
/**
 * 客户端连接服务器：telnet 127.0.0.1 6000；用来与服务器进行通信。
 * telnet 中退出客户端是使用 Ctrl+] 来进行退出发送消息！然后再输出 quit 退出 telnet。
 */

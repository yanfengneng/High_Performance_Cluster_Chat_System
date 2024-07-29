#include "chatserver.hpp"
#include "json.hpp"
#include "chatservice.hpp"

#include <functional>
#include <string>

using namespace std::placeholders;
using json = nlohmann::json;

/* 初始化聊天服务器对象 */
ChatServer::ChatServer(EventLoop* loop, const InetAddress& listenAddr,
                       const string& nameArg)
    : server_(loop, listenAddr, nameArg), loop_(loop) {
  // 注册链接回调函数：onConnection() 有一个参数，因此需要一个占位符
  server_.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1));

  // 注册消息回调：onMessage() 有三个参数，因此需要三个占位符
  server_.setMessageCallback(std::bind(&ChatServer::onMessage,this , _1, _2, _3));

  // 设置线程数量
  // 主 reactor 负责新用户的链接、断开，子 reactor 负责用户文件的读写
  server_.setThreadNum(4);
}

/* 开启事件循环 */
void ChatServer::start() { server_.start(); }

/* 上报链接相关信息的回调函数：链接创建、链接断开 */
void ChatServer::onConnection(const TcpConnectionPtr& conn) {
  // 客户端断开链接
  if(!conn->connected()){
    ChatService::instance()->clineCloseException(conn);
    conn->shutdown();
  }
}

/* 上报读写事件相关信息的回调函数 */
void ChatServer::onMessage(const TcpConnectionPtr& conn /* 连接 */,
                           Buffer* buffer /* 缓冲区 */,
                           Timestamp time /* 接收数据的时间信息 */) {
  std::string buf = buffer->retrieveAllAsString();
  // 数据的反序列化：解码
  json js = json::parse(buf);
  // 完全解耦网络模块的代码和业务模块的代码
  // 通过 js["msgid"] => 业务 handle => conn js time
  auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
  // 回调消息绑定好的事件处理器，来执行相应的业务处理
  msgHandler(conn, js, time);
}

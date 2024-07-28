#include "chatserver.hpp"
#include "chatservice.hpp"
#include <iostream>
#include <signal.h>

/* 处理服务器使用 ctrl+c 结束后，重置 user 的状态信息 */
void resetHandler(int){
  ChatService::instance()->reset();
  exit(0);
}

int main()
{
  signal(SIGINT, resetHandler);

  EventLoop loop;
  InetAddress addr("127.0.0.1", 6000);
  ChatServer server(&loop, addr, "ChatServer");

  server.start();
  loop.loop(); // 开启事件循环
  return 0;
}

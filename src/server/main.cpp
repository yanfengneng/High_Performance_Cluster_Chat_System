#include "chatserver.hpp"
#include "chatservice.hpp"
#include <iostream>
#include <signal.h>

/* 处理服务器使用 ctrl+c 结束后，重置 user 的状态信息 */
void resetHandler(int){
  ChatService::instance()->reset();
  exit(0);
}

int main(int argc, char **argv)
{
  if (argc < 3) {
    std::cerr << "command invalid! example: ./ChatServer 127.0.0.1 6000"
              << std::endl;
    exit(-1);
  }

  /* 解析通过命令行参数传递的 ip、port */
  char *ip = argv[1];
  uint16_t port = atoi(argv[2]);

  /* SIGINT 信号表示：中断进程信号 */
  signal(SIGINT, resetHandler);

  EventLoop loop;
  InetAddress addr(ip, port);
  ChatServer server(&loop, addr, "ChatServer");

  server.start();
  loop.loop(); // 开启事件循环
  return 0;
}

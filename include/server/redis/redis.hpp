#ifndef REDIS_H
#define REDIS_H

#include <hiredis/hiredis.h>

#include <functional>
#include <string>
#include <thread>

/* 使用 redis 作为集群聊天服务器通信的基于发布-订阅消息队列 */
class Redis {
 public:
  Redis();
  ~Redis();

  /* 连接 redis 服务器 */
  bool connect();

  /* 向 redis 指定的通道发布消息 */
  bool publish(int channel, std::string message);

  /* 向 redis 指定的通道订阅消息 */
  bool subscribe(int channel);

  /* 向 redis 指定的通道取消订阅消息 */
  bool unsubscribe(int channel);

  /* 在独立线程中接收订阅通道中的消息 */
  /* 专门响应通道上发生的消息的 */
  void observer_channel_message();

  /* 初始化向业务层上报通道消息的回调对象 */
  void init_notify_handler(std::function<void(int, std::string)> fn);

 private:
  /* 需要使用两个连接，因为订阅消息之后，当前的上下文就阻塞住了 */
  /* hiredis同步上下文对象，负责publish消息 */
  /* 相当于一个客户端 redis-cli 存储了连接相关的所有信息 */
  redisContext* publish_context_;

  /* hiredis同步上下文对象，负责subscribe消息 */
  /* 相当于令一个客户端 redis-cli 存储了连接相关的所有信息 */
  redisContext* subcribe_context_;

  /* 回调操作，收到订阅的消息，给 service 层上报 */
  std::function<void(int, std::string)> notify_message_handler_;
};

#endif

#include "redis.hpp"

#include <iostream>

/* 构造函数：将两个上下文指针置为空 */
Redis::Redis() : publish_context_(nullptr), subcribe_context_(nullptr) {}

/* 析构函数：释放资源 */
Redis::~Redis() {
  if (publish_context_ != nullptr) {
    redisFree(publish_context_);
  }

  if (subcribe_context_ != nullptr) {
    redisFree(subcribe_context_);
  }
}

bool Redis::connect() {
  /* 一个上下文就是一个连接环境 */
  /* 负责publish发布消息的上下文连接 */
  publish_context_ = redisConnect("127.0.0.1", 6379);
  if (nullptr == publish_context_) {
    std::cerr << "connect redis failed!" << std::endl;
    return false;
  }

  /* 负责subscribe订阅消息的上下文连接 */ 
  subcribe_context_ = redisConnect("127.0.0.1", 6379);
  if (nullptr == subcribe_context_) {
    std::cerr << "connect redis failed!" << std::endl;
    return false;
  }

  /* 在单独的线程中，监听通道上的事件，有消息给业务层进行上报 */
  std::thread t([&]() { observer_channel_message(); });
  t.detach();

  std::cout << "connect redis-server success!" << std::endl;

  return true;
}

/* 向 redis 指定的通道发布消息 */
bool Redis::publish(int channel, std::string message) {
  /* command 表示命令，相当于是敲了 publish 命令 */
  /**
   * redisCommand() 先把要发送的命令缓存到本地，然后调用 redisAppendCommand()， 
   */
  redisReply *reply = (redisReply *)redisCommand(
      publish_context_, "PUBLISH %d %s", channel, message.c_str());
  if (nullptr == reply) {
    std::cerr << "publish command failed!" << std::endl;
    return false;
  }
  /* 释放资源 */
  freeReplyObject(reply);
  return true;
}

/* 向redis指定的通道订阅消息 */
bool Redis::subscribe(int channel) {
  // SUBSCRIBE命令本身会造成线程阻塞等待通道里面发生消息，这里只做订阅通道，不接收通道消息
  // 通道消息的接收专门在observer_channel_message函数中的独立线程中进行
  // 只负责发送命令，不阻塞接收redis
  // server响应消息，否则和notifyMsg线程抢占响应资源
  if (REDIS_ERR ==
      redisAppendCommand(this->subcribe_context_, "SUBSCRIBE %d", channel)) {
    std::cerr << "subscribe command failed!" << std::endl;
    return false;
  }
  // redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕（done被置为1）
  int done = 0;
  while (!done) {
    if (REDIS_ERR == redisBufferWrite(this->subcribe_context_, &done)) {
      std::cerr << "subscribe command failed!" << std::endl;
      return false;
    }
  }
  // redisGetReply

  return true;
}

/* 向redis指定的通道取消订阅消息 */
bool Redis::unsubscribe(int channel) {
  if (REDIS_ERR ==
      redisAppendCommand(this->subcribe_context_, "UNSUBSCRIBE %d", channel)) {
    std::cerr << "unsubscribe command failed!" << std::endl;
    return false;
  }
  // redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕（done被置为1）
  int done = 0;
  while (!done) {
    if (REDIS_ERR == redisBufferWrite(this->subcribe_context_, &done)) {
      std::cerr << "unsubscribe command failed!" << std::endl;
      return false;
    }
  }
  return true;
}

// 在独立线程中接收订阅通道中的消息
void Redis::observer_channel_message() {
  redisReply *reply = nullptr;
  while (REDIS_OK == redisGetReply(this->subcribe_context_, (void **)&reply)) {
    // 订阅收到的消息是一个带三元素的数组
    if (reply != nullptr && reply->element[2] != nullptr &&
        reply->element[2]->str != nullptr) {
      // 给业务层上报通道上发生的消息
      notify_message_handler_(atoi(reply->element[1]->str),
                              reply->element[2]->str);
    }

    freeReplyObject(reply);
  }

  std::cerr << ">>>>>>>>>>>>> observer_channel_message quit <<<<<<<<<<<<<"
            << std::endl;
}

void Redis::init_notify_handler(std::function<void(int, std::string)> fn) {
  this->notify_message_handler_ = fn;
}

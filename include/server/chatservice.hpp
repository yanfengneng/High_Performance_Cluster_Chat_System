#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include <muduo/net/TcpConnection.h>

#include <functional>
#include <mutex>
#include <unordered_map>

#include "redis.hpp"
#include "friendmodel.hpp"
#include "groupmodel.hpp"
#include "json.hpp"
#include "offlinemessagemodel.hpp"
#include "usermodel.hpp"
using namespace muduo;
using namespace muduo::net;
using json = nlohmann::json;

/* 表示处理消息的事件回调方法类型 */
using MsgHandler =
    std::function<void(const TcpConnectionPtr& conn, json& js, Timestamp time)>;

/* 聊天服务器业务类 */
class ChatService {
 public:
  /* 获得单例对象的接口函数 */
  static ChatService* instance();
  /* 处理登录业务 */
  void login(const TcpConnectionPtr& conn, json& js, Timestamp time);
  /* 处理注册业务 */
  void reg(const TcpConnectionPtr& conn, json& js, Timestamp time);
  /* 一对一聊天业务 */
  void oneChat(const TcpConnectionPtr& conn, json& js, Timestamp time);
  /* 添加好友业务 */
  void addFriend(const TcpConnectionPtr& conn, json& js, Timestamp time);
  /* 创建群组业务 */
  void createGroup(const TcpConnectionPtr& conn, json& js, Timestamp time);
  /* 加入群组业务 */
  void addGroup(const TcpConnectionPtr& conn, json& js, Timestamp time);
  /* 群组聊天业务 */
  void groupChat(const TcpConnectionPtr& conn, json& js, Timestamp time);
  /* 处理注销业务 */
  void loginout(const TcpConnectionPtr& conn, json& js, Timestamp time);
  /* 服务器异常，业务重置方法 */
  void reset();
  /* 获取消息对应的处理器 */
  MsgHandler getHandler(int msgid);
  /* 处理客户端异常退出 */
  void clineCloseException(const TcpConnectionPtr& conn);
  /* 从 redis 消息队列中获取订阅的消息 */
  void handleRedisSubscribeMessage(int userid, std::string msg);

 private:
  /* 构造函数私有化：禁止外部构造 */
  ChatService();

  // 禁止外部析构
  ~ChatService() = default;

  // 禁止外部拷贝构造
  ChatService(const ChatService& charService) = delete;

  // 禁止外部赋值操作
  const ChatService& operator=(const ChatService& charService) = delete;

  // 消息处理表：存储消息 id 和其对应的业务处理方法
  std::unordered_map<int, MsgHandler> msgHandlerMap_;

  // 定义互斥锁，保证 userConnMap_ 的线程安全
  std::mutex connMutex_;
  // 存储在线用户的通信连接表
  // 需要注意线程安全
  std::unordered_map<int, TcpConnectionPtr> userConnMap_;

  // 数据操作类对象
  UserModel userModel_;
  OfflineMsgModel offlineMsgModel_;
  FriendModel friendModel_;
  GroupModel groupModel_;

  // redis 操作对象
  Redis redis_;
};

#endif

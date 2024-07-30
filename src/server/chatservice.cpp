#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include <vector>
#include <iostream>

using namespace muduo;

/* 获得单例对象的接口函数 */
ChatService* ChatService::instance() {
  static ChatService service;
  return &service;
}

ChatService::ChatService() {
  /* 1. 用户基本业务管理相关事件处理回调注册 */
  msgHandlerMap_.insert(
      {LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
  msgHandlerMap_.insert(
      {LOGINOUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});
  msgHandlerMap_.insert(
      {REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
  msgHandlerMap_.insert(
      {ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
  msgHandlerMap_.insert(
      {ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});

  /* 2. 群组业务管理相关事件处理回调注册 */
  msgHandlerMap_.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup,
                                                     this, _1, _2, _3)});
  msgHandlerMap_.insert(
      {ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
  msgHandlerMap_.insert(
      {GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

  /* 3. 连接 redis 服务器 */
  if(redis_.connect()){
    /* 设置上报消息的回调 */
    redis_.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
  }
}

/* 服务器异常，业务重置方法 */
void ChatService::reset(){
  // 把 online 状态的用户，设置成 offline
  userModel_.resetState();
}

/* 获取消息对应的处理器 */
MsgHandler ChatService::getHandler(int msgid) {
  // 记录错误日志：msgid 没有对应的事件处理回调
  // 没有就返回空操作
  if (msgHandlerMap_.find(msgid) == msgHandlerMap_.end()) {
    // 返回一个空操作
    return [=](const TcpConnectionPtr& conn, json& js, Timestamp time) {
      LOG_ERROR << "msgid: " << msgid << " can't find handler!";
    };
  }
  // 有就返回存在的操作
  return msgHandlerMap_[msgid];
}

/* 处理登录业务：输入 id 和 密码 就行啦！还需要检查下 id 对应的 pwd 是否正确。 */
void ChatService::login(const TcpConnectionPtr& conn, json& js,
                        Timestamp time) {
  // std::cout << js << std::endl;
  int id = js["id"].get<int>();
  std::string pwd = js["password"];

  User user = userModel_.query(id);
  // std::cout << js << std::endl;
  if (user.getId() == id && user.getPassword() == pwd) {
    if (user.getState() == "online") {
      json response;
      response["msgid"] = LOGIN_MSG_ACK;
      // 1 表示有错
      response["errno"] = 1;
      response["errmsg"] = "This accout is using, input another!";
      // 发送序列化数据
      conn->send(response.dump());
    } else {
      // 登录成功，记录用户链接信息
      // 设计多线程访问，需要考虑其线程安全问题
      {
        // 在该作用域加锁，出了该作用域就解锁了
        std::lock_guard<std::mutex> lock(connMutex_);
        userConnMap_.insert({id, conn});
      }

      // id用户登录成功后，向 redis 订阅 channel(id)
      redis_.subscribe(id);

      // 登录成功：更新用户的状态信息 state：offline => online
      user.setState("online");
      userModel_.updateState(user); 

      json response;
      response["msgid"] = LOGIN_MSG_ACK;
      // 0 表示没错
      response["errno"] = 0;
      response["id"] = user.getId();
      response["name"] = user.getName();

      /* 查询该用户是否有离线消息 */
      std::vector<std::string> vec = offlineMsgModel_.query(id);
      if (!vec.empty()) {
        response["offlinemsg"] = vec;
        // 读取该用户的离线消息后，把该用户的所有离线消息删除掉
        offlineMsgModel_.remove(id);
      }

      /* 查询该用户的好友信息并返回 */ 
      std::vector<User> userVec = friendModel_.query(id);
      if(!userVec.empty()){
        std::vector<std::string> vec2;
        for(auto user: userVec){
          json js;
          js["id"] = user.getId();
          js["name"] = user.getName();
          js["state"] = user.getState();
          // dump() 函数是将 json 序列化为字符串
          vec2.push_back(js.dump());
        }
        response["friends"] = vec2;
      }

      /* 查询用户的群组信息 */
      std::vector<Group> groupuserVec = groupModel_.queryGroups(id);
      if (!groupuserVec.empty()) {
        // group:[{groupid:[xxx, xxx, xxx, xxx]}]
        std::vector<std::string> groupV;
        for (Group& group : groupuserVec) {
          json grpjson;
          grpjson["id"] = group.getId();
          grpjson["groupname"] = group.getName();
          grpjson["groupdesc"] = group.getDesc();
          std::vector<std::string> userV;
          for (GroupUser& user : group.getUsers()) {
            json js;
            js["id"] = user.getId();
            js["name"] = user.getName();
            js["state"] = user.getState();
            js["role"] = user.getRole();
            userV.push_back(js.dump());
          }
          grpjson["users"] = userV;
          groupV.push_back(grpjson.dump());
        }
        response["groups"] = groupV;
      }
      // 发送序列化数据
      conn->send(response.dump());
    }
  } else {
    // 登陆失败：该用户不存在；用户存在但是密码错误
    json response;
    response["msgid"] = LOGIN_MSG_ACK;
    // 1 表示有错
    response["errno"] = 1;
    response["errmsg"] = "Id or password is invalid!";
    // 发送序列化数据
    conn->send(response.dump());
  }
}

/* 处理注册业务：name, pwd */
void ChatService::reg(const TcpConnectionPtr& conn, json& js, Timestamp time) {
  std::string name = js["name"];
  std::string pwd = js["password"];

  User user;
  user.setName(name);
  user.setPassword(pwd);

  bool state = userModel_.insert(user);
  if (state) {
    // 注册成功
    json response;
    response["msgid"] = REG_MSG_ACK;
    // 0 表示没错
    response["errno"] = 0;
    response["id"] = user.getId();
    // 发送序列化数据
    conn->send(response.dump());
    
  } else {
    // 注册失败
    json response;
    response["msgid"] = REG_MSG_ACK;
    // 1 表示有错
    response["errno"] = 1;
    response["id"] = user.getId();
    // 发送序列化数据
    conn->send(response.dump());
  }
}

/* 处理客户端异常退出 */
void ChatService::clineCloseException(const TcpConnectionPtr& conn) {
  User user;
  {
    // 仅仅只有删除用户的连接信息需要加锁，更新用户信息不需要加锁
    // 因此删除用户连接信息部分单独放在一块作用域内，出了该作用域就释放锁了
    std::lock_guard<std::mutex> lock(connMutex_);
    for (auto it = userConnMap_.begin(); it != userConnMap_.end(); ++it) {
      if (it->second == conn) {
        user.setId(it->first);
        // 从 map 表中删除用户的连接信息
        userConnMap_.erase(it);
        break;
      }
    }
  }

  // 用户注销，相当于就是下线，在 redis 中取消订阅通道
  redis_.unsubscribe(user.getId());

  // 要是没有找到用户信息的话，就不用在数据表中进行操作了
  // 更新用户的状态信息
  if (user.getId() != -1) {
    user.setState("offline");
    userModel_.updateState(user);
  }
}

/* 一对一聊天业务 */
void ChatService::oneChat(const TcpConnectionPtr& conn, json& js,
                          Timestamp time) {
  // 获得对方的 id
  int toid = js["toid"].get<int>();
  {
    /* 该作用域表示用户 A、B 在同一台机器上面 */
    // 所只要离开作用域就会被析构，释放锁
    std::lock_guard<std::mutex> lock(connMutex_);
    auto it = userConnMap_.find(toid);
    if (it != userConnMap_.end()) {
      // toid 在线，转发消息：服务器主动推送消息给 toid 用户
      it->second->send(js.dump());
      return;
    }
  }

  /* 用户 A、B 不在同一台电脑上登录 */
  // 查询 toid 是否在线
  User user = userModel_.query(toid);
  if (user.getState() == "online") {
    /* 发布消息 */
    redis_.publish(toid, js.dump());
    return;
  }
  // toid 不在线，存储离线消息
  offlineMsgModel_.insert(toid, js.dump());
}

/* 添加好友业务：msgid、id、friendid */
void ChatService::addFriend(const TcpConnectionPtr& conn, json& js, Timestamp time){
  int userid = js["id"].get<int>();
  int friendid = js["friendid"].get<int>();

  /* 存储好友信息 */
  friendModel_.insert(userid, friendid);
}

/* 创建群组业务 */
void ChatService::createGroup(const TcpConnectionPtr& conn, json& js, Timestamp time){
  int userid = js["id"].get<int>();
  std::string name = js["groupname"];
  std::string desc = js["groupdesc"];

  /* 存储新创建的群组信息 */
  Group group(-1, name, desc);
  if(groupModel_.createGroup(group)){
    /* 存储群组创建人信息 */
    groupModel_.addGroup(userid, group.getId(), "creator");
  }
}

/* 加入群组业务 */
void ChatService::addGroup(const TcpConnectionPtr& conn, json& js, Timestamp time){
  int userid = js["id"].get<int>();
  int groupid = js["groupid"].get<int>();
  groupModel_.addGroup(userid, groupid, "normal");
}

/* 群组聊天业务 */
void ChatService::groupChat(const TcpConnectionPtr& conn, json& js, Timestamp time){
  int userid = js["id"].get<int>();
  int groupid = js["groupid"].get<int>();
  /* 获得该群的其他所有群友 id */
  std::vector<int> useridVec = groupModel_.queryGroupUsers(userid, groupid);
  /* 加锁：增加线程安全 */
  std::lock_guard<std::mutex> lock(connMutex_);
  for (int id : useridVec) {
    auto it = userConnMap_.find(id);
    if (it != userConnMap_.end()) {
      /* 转发消息 */
      it->second->send(js.dump());
    } else {
      /* 查询 toid 是否在线 */
      User user = userModel_.query(id);
      if (user.getState() == "online") {
        /* redis 发布消息 */
        redis_.publish(id, js.dump());
      } else {
        /* 存储离线消息 */
        offlineMsgModel_.insert(id, js.dump());
      }
    }
  }
}

/* 处理注销业务 */
void ChatService::loginout(const TcpConnectionPtr& conn, json& js,
                           Timestamp time) {
  int userid = js["id"].get<int>();
  {
    /* 加锁：增加线程安全 */
    std::lock_guard<std::mutex> lock(connMutex_);
    auto it = userConnMap_.find(userid);
    if (it != userConnMap_.end()) {
      /* 用户 id 存在于用户通信连接表中，则进行删除 */
      userConnMap_.erase(it);
    }
  }

  // 用户注销，相当于就是下线，在 redis 中取消订阅通道
  redis_.unsubscribe(userid);

  // 更新用户的状态信息
  User user(userid, "", "", "offline");
  userModel_.updateState(user);
}

/* 从redis消息队列中获取订阅的消息 */
void ChatService::handleRedisSubscribeMessage(int userid, std::string msg) {
  std::lock_guard<std::mutex> lock(connMutex_);
  auto it = userConnMap_.find(userid);
  if (it != userConnMap_.end()) {
    it->second->send(msg);
    return;
  }
  // 存储该用户的离线消息
  offlineMsgModel_.insert(userid, msg);
}

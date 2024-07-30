#include <chrono>
#include <ctime>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "json.hpp"

using json = nlohmann::json;

#include <arpa/inet.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <atomic>

#include "group.hpp"
#include "public.hpp"
#include "user.hpp"

User g_currentUser;  // 记录当前系统登陆的用户信息
std::vector<User> g_currentUserFriendList;  // 记录当前用户的好友列表信息
std::vector<Group> g_currentUserGroupList;  // 记录当前登录用户的群组列表信息
bool isMainMenuRunning = false;            // 控制主菜单页面程序
sem_t rwsem;                               // 用户读写线程之间的通信
std::atomic_bool g_isLoginSuccess{false};  // 记录登录状态是否成功

/* command handler：命令处理 */
/* "help" command handler */
void help(int fd = 0, std::string str = "");
/* "chat" command handler */
void chat(int, std::string);
/* "addfriend" command handler */
void addfriend(int, std::string);
/* "creategroup" command handler */
void creategroup(int, std::string);
/* "addgroup" command handler */
void addgroup(int, std::string);
/* "groupchat" command handler */
void groupchat(int, std::string);
/* "loginout" command handler */
void loginout(int, std::string);

/* 聊天系统支持的客户端命令列表 */
std::unordered_map<std::string, std::string> commandMap = {
    {"help", "显示所有支持的命令，格式 help"},
    {"chat", "一对一聊天，格式 chat:friendid:message"},
    {"addfriend", "添加好友，格式 addfriend:friendid"},
    {"creategroup", "创建群组，格式 creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组，格式 addgroup:groupid"},
    {"groupchat", "群聊，格式 groupchat:groupid:message"},
    {"loginout", "注销，格式 loginout"}};

/* 注册系统支持的客户端命令处理 */
std::unordered_map<std::string, std::function<void(int, std::string)>>
    commandHandlerMap = {{"help", help},           {"chat", chat},
                         {"addfriend", addfriend}, {"creategroup", creategroup},
                         {"addgroup", addgroup},   {"groupchat", groupchat},
                         {"loginout", loginout}};

/* 1. 显示当前登录成功用户的基本信息 */
void showCurrentUserData();
/* 2. 接收线程 */
void readTaskHandler(int clientfd);
/* 3. 获取系统时间（聊天信息需要添加时间信息） */
std::string getCurrentTime();
/* 4. 主聊天页面程序 */
void mainMenu(int clientfd);
/* 5. login：处理登录的响应逻辑 */
void doLoginResponse(json &responsejs);
/* 6. register：处理注册的相应逻辑 */
void doRegResponse(json &responsejs);

/* main: 聊天客服端程序实现，main 线程用作发送线程，子线程用作接收线程 */
int main(int argc, char **argv) {
  if (argc < 3) {
    std::cerr << "Command invalid! example: ./ChatClient 127.0.0.1 6000"
              << std::endl;
    exit(-1);
  }
  /* main1. 解析通过命令行传递的参数：ip 和 port */
  char *ip = argv[1];
  uint16_t port = std::atoi(argv[2]);

  /* main2. 创建 client 端的 socket */
  int clientfd = socket(AF_INET, SOCK_STREAM, 0);
  if (-1 == clientfd) {
    std::cerr << "Socket create error..." << std::endl;
    exit(-1);
  }

  /* main3. 填写 client 需要连接的 server 的信息：ip + port */
  sockaddr_in server;
  memset(&server, 0, sizeof(sockaddr_in));
  server.sin_family = AF_INET;
  server.sin_port = htons(port);  // 客户端连接的服务器端口号
  server.sin_addr.s_addr = inet_addr(ip);  // 客户端连接的服务器 ip 地址

  /* main4. client 和 server 进行连接 */
  if (-1 == connect(clientfd, (sockaddr *)&server, sizeof(sockaddr_in))) {
    std::cerr << "Connect server error..." << std::endl;
    close(clientfd);
    exit(-1);
  }

  /* main5. 初始化信号量 */
  sem_init(&rwsem, 0, 0);

  /* main6. 连接服务器成功，启动接收线程负责接收数据 */
  /* 在 linux 上相当于底层调用了 pthread_create() */
  std::thread readTask(readTaskHandler, clientfd);
  /* 在 linux 上相当于底层调用了 pthread_detach() */
  readTask.detach();  // 线程分离

  /* main7. 线程用于接收用于输入，负责发送数据 */
  for (;;) {
    /* 显示首页面菜单 登录、注册、退出 */
    std::cout << "========================" << std::endl;
    std::cout << "1. login" << std::endl;
    std::cout << "2. register" << std::endl;
    std::cout << "3. quit" << std::endl;
    std::cout << "========================" << std::endl;
    std::cout << "choice:";
    int choice = 0;
    std::cin >> choice;
    /* 读掉缓冲区残留的回车，如果不回收到回车，后面在输入就会读取回车，不读取输入的数据
     */
    std::cin.get();
    switch (choice) {
      case 1: { /* login 业务 */
        int id = 0;
        char pwd[50] = {0};
        std::cout << "userid:";
        std::cin >> id;
        std::cin.get();  // 读掉缓冲区残留的回车
        std::cout << "userpassword:";
        std::cin.getline(pwd, 50);

        json js;
        js["msgid"] = LOGIN_MSG;
        js["id"] = id;
        js["password"] = pwd;
        std::string request = js.dump();

        g_isLoginSuccess = false;

        int len =
            send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
        if (len == -1) {
          std::cerr << "send login msg error:" << request << std::endl;
        }
        
        sem_wait(&rwsem); // P操作：等待信号量，由子进程处理完登录的响应消息后，通知这里
        if(g_isLoginSuccess){
          /* 进入聊天主菜单页面 */
          isMainMenuRunning = true;
          mainMenu(clientfd);
        }
        break;
      }
      case 2: { /* register 业务 */
        /* 注册业务需要填写账号、密码 */
        char name[50] = {0};
        char pwd[50] = {0};
        std::cout << "username:";
        std::cin.getline(name, 50);  // 遇见回车结束
        std::cout << "userpassword:";
        std::cin.getline(pwd, 50);
        // std::cout << "1:" << name << " " << pwd << std::endl;
        json js;
        js["msgid"] = REG_MSG;
        js["name"] = name;
        js["password"] = pwd;
        // std::cout << "2:" << name << " " << pwd << std::endl;
        // 账号密码读取的没问题啊
        // std::cout << "Debug: JSON object = " << js << std::endl;
        /* json 对象序列化成字符串 */
        std::string request = js.dump();
        int len =
            send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
        if (len == -1) {
          std::cerr << "Send reg msg error:" << request << std::endl;
        }
        /* 等待信号量，子线程处理完注册消息会通知 */
        sem_wait(&rwsem); // P操作：申请信号量
        // break 在大括号 {} 里面，依旧会跳出 switch 语句
        // 因为大括号只是创建了一个作用域，不会改变 break 的行为。
        // break 语句会跳出包含它最外层的 switch、for、while、do 循环。
        break;
      }
      case 3: /* quit 业务 */
        close(clientfd);
        sem_destroy(&rwsem); /* 销毁信号量 */
        exit(0);
      default:
        std::cerr << "Invalid input!" << std::endl;
        break;
    }
  }
  return 0;
}

/* 1. 显示当前登录成功用户的基本信息 */
void showCurrentUserData() {
  /* 1.1 打印用户个人信息 */
  std::cout << "======================login user======================"
            << std::endl;
  std::cout << "current login user => id: " << g_currentUser.getId()
            << " name:" << g_currentUser.getName() << std::endl;

  /* 1.2 打印好友列表 */
  std::cout << "----------------------friend list---------------------"
            << std::endl;
  if (!g_currentUserFriendList.empty()) {
    for (User &user : g_currentUserFriendList) {
      std::cout << user.getId() << " " << user.getName() << " "
                << user.getState() << std::endl;
    }
  }

  /* 1.3 打印该用户的群组信息 */
  std::cout << "----------------------group list----------------------"
            << std::endl;
  if (!g_currentUserGroupList.empty()) {
    for (Group &group : g_currentUserGroupList) {
      std::cout << "群号：" << group.getId() << ", 群名: " << group.getName() << ", 群描述："
                << group.getDesc() << std::endl;
      std::cout << "群友信息如下：" << std::endl;
      for (GroupUser &user : group.getUsers()) {
        std::cout << user.getId() << " " << user.getName() << " "
                  << user.getState() << " " << user.getRole() << std::endl;
      }
      std::cout << "======================================================"
            << std::endl;
    }
  }
  std::cout << "======================================================"
            << std::endl;
}

/* 2. 接收线程 */
void readTaskHandler(int clientfd) {
  for (;;) {
    char buffer[1024] = {0};
    /* 接收服务器发送来的数据 */
    int len = recv(clientfd, buffer, 1024, 0); // 阻塞了
    if (-1 == len || 0 == len) { /* 接收数据失败或者接收到空数据 */
      close(clientfd);
      exit(-1);
    }
    /* 反序列化：将字符串解析为 json 对象 */
    /* 接收 ChatServer 转发的数据，反序列化生成 json 数据对象 */
    json js = json::parse(buffer);
    int msg_type = js["msgid"].get<int>();
    /* 个人与个人之间的聊天 */
    if (ONE_CHAT_MSG == msg_type) {
      std::cout << js["time"].get<std::string>() << " [" << js["id"] << "]"
                << js["name"].get<std::string>()
                << " said: " << js["msg"].get<std::string>() << std::endl;
      continue;
    }

    /* 群聊 */
    if (GROUP_CHAT_MSG == msg_type) {
      std::cout << "群消息[" << js["groupid"]
                << "]:" << js["time"].get<std::string>() << " [" << js["id"]
                << "]" << js["name"].get<std::string>()
                << " said: " << js["msg"].get<std::string>() << std::endl;
      continue;
    }

    /* 登录响应消息 */
    if (LOGIN_MSG_ACK == msg_type) {
      doLoginResponse(js); /* 处理登录响应的业务逻辑 */
      // V 操作：释放信号量
      sem_post(&rwsem); /* 通知主线程，登录结果处理完成 */
      continue;
    }

    /* 注册响应消息 */
    if (REG_MSG_ACK == msg_type) {
      doRegResponse(js); /* 处理注册响应的业务逻辑 */
      // V 操作：释放信号量
      sem_post(&rwsem); /* 通知主线程，注册结果处理完成 */
      continue;
    }
  }
}

/* 3. 获取系统时间（聊天信息需要添加时间信息） */
std::string getCurrentTime() {
  auto tt =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  struct tm *ptm = localtime(&tt);
  char date[60] = {0};
  /* sprintf() 函数是将格式化的数据写入到字符数组 date 中 */
  sprintf(date, "%d-%02d-%02d %02d:%02d:%02d", (int)ptm->tm_year + 1900,
          (int)ptm->tm_mon + 1, (int)ptm->tm_mday, (int)ptm->tm_hour,
          (int)ptm->tm_min, (int)ptm->tm_sec);
  /* 返回字符串的时间 */
  return std::string(date);
}

/* 4. 主聊天页面程序 */
void mainMenu(int clientfd) {
  help();

  char buffer[1024] = {0};
  while (isMainMenuRunning) {
    /* 读取输入字符串，以回车符为结束符 */
    std::cin.getline(buffer, 1024);
    std::string commandbuf(buffer);
    std::string command; /* 存储命令 */
    int idx = commandbuf.find(":");
    if (-1 == idx) { // 没有找到":"
      command = commandbuf;
    } else {// 找到":"
      command = commandbuf.substr(0, idx);
    }
    auto it = commandHandlerMap.find(command);
    if (it == commandHandlerMap.end()) {
      std::cerr << "Invalid input command!" << std::endl;
      continue;
    }
    /* 调用相应命令的事件处理回调，mainMenu
     * 对修改封闭，添加新功能不需要修改该函数 */
    /* substr(起始位置，截取字符串的长度) */
    it->second(clientfd, commandbuf.substr(idx + 1, commandbuf.size() - idx));
  }
}

/* 5. login：处理登录的响应逻辑 */
void doLoginResponse(json &responsejs) {
  if (0 != responsejs["errno"].get<int>()) { /* 登录失败 */
    std::cerr << responsejs["errmsg"] << std::endl;
    g_isLoginSuccess = false;
  } else { /* 登录成功 */
    /* 记录当前用户的 id 和 name */
    g_currentUser.setId(responsejs["id"].get<int>());
    g_currentUser.setName(responsejs["name"]);

    /* 记录当前用户的好友列表信息 */
    if (responsejs.contains("friends")) {
      /* 先清空之前用户的好友列表，然后再添加当前用户的好友列表 */
      g_currentUserFriendList.clear();
      // friends 中存的是每个用户的信息，因此需要遍历解析每个用户的信息
      std::vector<std::string> vec = responsejs["friends"];
      for (std::string &str : vec) {
        // 反序列化：字符串反序列化成 json 对象
        json js = json::parse(str);
        User user;
        user.setId(js["id"].get<int>());
        user.setName(js["name"]);
        user.setState(js["state"]);
        g_currentUserFriendList.push_back(user);
      }
    }

    /* 记录当前用户的群组列表信息 */
    /* 服务端是怎么打包的，客户端就怎么解析 */
    if (responsejs.contains("groups")) {
      // groups 中存的是每个组的信息，每个组里面还需要存群友的信息
      // 因此在每个组中还要遍历群友信息
      std::vector<std::string> vec1 = responsejs["groups"];
      for (std::string &groupstr : vec1) {/* 遍历每个组 */
        json groupjs = json::parse(groupstr);
        Group group;
        group.setId(groupjs["id"].get<int>());
        group.setName(groupjs["groupname"]);
        group.setDesc(groupjs["groupdesc"]);

        // 群里面的所有用户
        std::vector<std::string> vec2 = groupjs["users"];
        for (std::string &userstr : vec2) {/* 遍历组员信息 */
          GroupUser user;
          /* 字符串反序列化为 json 对象 */
          json js = json::parse(userstr);
          user.setId(js["id"].get<int>());
          user.setName(js["name"]);
          user.setState(js["state"]);
          user.setRole(js["role"]);
          group.getUsers().push_back(user);
        }
        g_currentUserGroupList.push_back(group);
      }
    }

    /* 显示登录用户的基本信息 */
    showCurrentUserData();

    /* 显示当前用户的离线消息：个人聊天信息或者群组消息 */
    if (responsejs.contains("offlinemsg")) {
      std::vector<std::string> vec = responsejs["offlinemsg"];
      for (std::string &str : vec) {
        json js = json::parse(str);
        /* 个人与个人之间的聊天 */
        if (ONE_CHAT_MSG == js["msgid"].get<int>()) {
          /* time + [id] + name + " said: " + xxx */
          std::cout << js["time"].get<std::string>() << " [" << js["id"] << "]"
                    << js["name"].get<std::string>()
                    << " said: " << js["msg"].get<std::string>() << std::endl;

        } else { /* 群聊 */
          std::cout << "群消息[" << js["groupid"]
                    << "]:" << js["time"].get<std::string>() << " [" << js["id"]
                    << "]" << js["name"].get<std::string>()
                    << " said: " << js["msg"].get<std::string>() << std::endl;
        }
      }
    }
    /* 当前用户登录成功 */
    g_isLoginSuccess = true;
  }
}

/* 6. register：处理注册的相应逻辑 */
void doRegResponse(json &responsejs) {
  if (0 != responsejs["errno"].get<int>()) { /* 注册失败 */
    std::cerr << "Name is already exist, register error!" << std::endl;
  } else { /* 注册成功 */
    std::cout << "Name register success, userid is " << responsejs["id"]
              << ", do not forget it!" << std::endl;
  }
}

/* "help" command handler */
void help(int, std::string) {
  std::cout << "show command list >>> " << std::endl;
  for (auto &p : commandMap) {
    std::cout << p.first << " : " << p.second << std::endl;
  }
  std::cout << std::endl;
}

/* "addfriend" command handler */
void addfriend(int clientfd, std::string str) {
  int friendid = std::atoi(str.c_str());
  json js;
  js["msgid"] = ADD_FRIEND_MSG;
  js["id"] = g_currentUser.getId();
  js["friendid"] = friendid;
  /* json 对象序列化为字符串 */
  std::string buffer = js.dump();
  /* 向服务器发送消息 */
  int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
  if (-1 == len) {
    std::cerr << "Send addfriend msg error -> " << buffer << std::endl;
  }
}

/* "chat" command handler */
void chat(int clientfd, std::string str) {
  int idx = str.find(":");  /* friendid:message */
  if (-1 == idx) {
    std::cerr << "Chat command invalid!" << std::endl;
    return;
  }

  int friendid = std::atoi(str.substr(0, idx).c_str()); // 好友id
  std::string message = str.substr(idx + 1, str.size() - idx); // 消息

  json js;
  js["msgid"] = ONE_CHAT_MSG;
  js["id"] = g_currentUser.getId();
  js["name"] = g_currentUser.getName();
  js["toid"] = friendid;
  js["msg"] = message;
  js["time"] = getCurrentTime(); /* 获得当前时间 */
  /* 序列化为字符串 */
  std::string buffer = js.dump();

  int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
  if (-1 == len) {
    std::cerr << "Send chat msg error -> " << buffer << std::endl;
  }
}

/* "creategroup" command handler => groupname:groupdesc */
void creategroup(int clientfd, std::string str) {
  int idx = str.find(":");
  if (-1 == idx) {
    std::cerr << "Creategroup command invalid!" << std::endl;
    return;
  }
  // substr(起始位置，截取字符串的长度)
  std::string groupname = str.substr(0, idx); // 群名
  std::string groupdesc = str.substr(idx + 1, str.size() - idx); // 群描述

  json js;
  js["msgid"] = CREATE_GROUP_MSG;
  js["id"] = g_currentUser.getId(); // 当前用户的 id
  js["groupname"] = groupname;
  js["groupdesc"] = groupdesc;
  /* json 对象序列化为字符串 */
  std::string buffer = js.dump();

  int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
  if (-1 == len) {/* 发送字符串失败 */
    std::cerr << "Send creategroup msg error -> " << buffer << std::endl;
  }
}

/* "addgroup" command handler */
void addgroup(int clientfd, std::string str) {
  int groupid = std::atoi(str.c_str()); // 群号
  json js;
  js["msgid"] = ADD_GROUP_MSG;
  js["id"] = g_currentUser.getId();
  js["groupid"] = groupid;
  /* json 对象序列化为字符串 */
  std::string buffer = js.dump();

  int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
  if (-1 == len) {/* 发送字符串失败 */
    std::cerr << "Send addgroup msg error -> " << buffer << std::endl;
  }
}

/* "groupchat" command handler => groupid:message */
void groupchat(int clientfd, std::string str) {
  int idx = str.find(":");
  if (-1 == idx) {
    std::cerr << "Groupchat command invalid!" << std::endl;
    return;
  }

  int groupid = std::atoi(str.substr(0, idx).c_str()); // 群号
  std::string message = str.substr(idx + 1, str.size() - idx); // 群消息

  json js;
  js["msgid"] = GROUP_CHAT_MSG;
  js["id"] = g_currentUser.getId();
  js["name"] = g_currentUser.getName();
  js["groupid"] = groupid;
  js["msg"] = message;
  js["time"] = getCurrentTime();
  /* json 对象序列化为字符串 */
  std::string buffer = js.dump();

  int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
  if (-1 == len) {/* 发送字符串失败 */
    std::cerr << "Send groupchat msg error -> " << buffer << std::endl;
  }
}

/* "loginout" command handler */
void loginout(int clientfd, std::string) {
  json js;
  js["msgid"] = LOGINOUT_MSG;
  js["id"] = g_currentUser.getId();
  /* json 对象序列化为字符串 */
  std::string buffer = js.dump();

  int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
  if (-1 == len) {/* 发送字符串失败 */
    std::cerr << "Send loginout msg error -> " << buffer << std::endl;
  } else {
    isMainMenuRunning = false;
  }
}

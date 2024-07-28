#ifndef GROUPUSER_H
#define GROUPUSER_H

#include "user.hpp"

/* 群组用户：多了一个 role 角色信息，从 User 类直接继承，复用 User 的其他信息 */
/* 对应 GroupUser 表 */
class GroupUser : public User {
 public:
  void setRole(std::string role) { this->role = role; }
  std::string getRole() { return this->role; }

 private:
  /* 派生类的特殊变量 role 角色 */
  std::string role;
};

#endif

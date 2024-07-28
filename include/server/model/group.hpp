#ifndef GROUP_H
#define GROUP_H

#include <string>
#include <vector>

#include "groupuser.hpp"
 
/* Group 表的 ORM 类 */
/* 对应 allgroup 表 */
class Group {
 public:
  Group(int id = -1, std::string name = "", std::string desc = "") {
    this->id = id;
    this->name = name;
    this->desc = desc;
  }

  void setId(int id) { this->id = id; }
  void setName(std::string name) { this->name = name; }
  void setDesc(std::string desc) { this->desc = desc; }

  int getId() { return this->id; }
  std::string getName() { return this->name; }
  std::string getDesc() { return this->desc = desc; }  // 组功能描述
  // 返回群成员列表：群成员在 user 的基础上多了个群角色（管理员、群友）
  std::vector<GroupUser> &getUsers() { return this->users; }

 private:
  int id;
  std::string name;
  std::string desc;
  std::vector<GroupUser> users;  // 组成员的信息
};

#endif

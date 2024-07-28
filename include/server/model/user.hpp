#ifndef USER_H
#define USER_H

#include <string>

/* User 表的 ORM 类：用来映射表的字段的 */
class User {
 public:
  User(int id = -1, std::string name = "", std::string pwd = "",
       std::string state = "offline")
      : id_(id), name_(name), password_(pwd), state_(state) {}

  void setId(int id) { id_ = id; }
  void setName(const std::string& name) { name_ = name; }
  void setPassword(const std::string& pwd) { password_ = pwd; }
  void setState(const std::string& state) { state_ = state; }

  int getId() { return id_; }
  std::string getName() { return name_; }
  std::string getPassword() { return password_; }
  std::string getState() { return state_; }

 private:
  int id_;
  std::string name_;
  std::string password_;
  std::string state_;
};

#endif

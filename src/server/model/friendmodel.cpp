#include "friendmodel.hpp"
#include "db.h"

/* 添加好友关系 */
void FriendModel::insert(int userid, int friendid) {
  // 1. 组装 sql 语句
  char sql[1024] = {0};
  sprintf(sql, "insert into friend values('%d', '%d')", userid, friendid);
  MySQL mysql;
  if (mysql.connect()) {
    mysql.update(sql);
  }
}

/* 返回用户好友列表： */
std::vector<User> FriendModel::query(int userid) {
  // 1. 组装 sql 语句
  char sql[1024] = {0};
  sprintf(sql,
          "select a.id, a.name, a.state from user a inner join friend b on "
          "b.userid = a.id where b.userid = %d;",
          userid);
  MySQL mysql;
  std::vector<User> vec;
  if (mysql.connect()) {
    MYSQL_RES* res = mysql.query(sql);
    // 把 userid 用户的所有离线消息放入 vec 中返回
    if (res != nullptr) {
      MYSQL_ROW row;
      // 使用循环，因为数据可能有多条
      while ((row = mysql_fetch_row(res)) != nullptr) {
        User user;
        user.setId(std::atoi(row[0]));
        user.setName(row[1]);
        user.setState(row[2]);
        vec.push_back(user);
      }
      mysql_free_result(res);
      return vec;
    }
  }
  return vec;
}

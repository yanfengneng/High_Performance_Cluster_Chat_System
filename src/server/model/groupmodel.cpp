#include "groupmodel.hpp"

#include "db.h"

/* 创建群组 */
bool GroupModel::createGroup(Group &group) {
  // 组装sql语句
  char sql[1024] = {0};
  /* 需要字符串char user.getName()获得是 string 所以调用c_str() 插入数据 */
  /* 插入一条记录，自动生成主键 id，这里使用自增的 id */
  sprintf(sql, "insert into allgroup(groupname, groupdesc) values('%s', '%s')",
          group.getName().c_str(), group.getDesc().c_str());

  MySQL mysql;
  // 连接数据库
  if (mysql.connect()) {
    // 更新数据库语句
    if (mysql.update(sql)) {
      /* mysql_insert_id(mysql.getConnection() 获得主键 id */
      /* 然后再把实参 group 的群号  groupid 修改正确 */
      group.setId(mysql_insert_id(mysql.getConnection()));
      return true;
    }
  }
  return false;
}

/* 加入群组 */
void GroupModel::addGroup(int userid, int groupid, std::string role) {
  char sql[1024] = {0};
  /* 需要字符串char user.getName()获得是string 所以调用c_str() 插入数据 */
  sprintf(sql, "insert into groupuser values(%d, %d,'%s')", groupid, userid,
          role.c_str());
  MySQL mysql;
  /* 连接数据库 */
  if (mysql.connect()) {
    /* 更新数据库语句 */
    mysql.update(sql);
  }
}

/* 查询用户所在群组信息：用户登录成功以后，要返回群组信息以及群成员用户信息 */
std::vector<Group> GroupModel::queryGroups(int userid) {
  /**
   * 业务解释：
   * 1. 首先根据 userid 在 groupuser 表中查看到该用户的所有群组消息：
   *    这里群组消息需要两张表 allgroup 和 groupuser，
   *    使用 userid 来查到该用户的所有群号，
   *    然后联合 allgroup 和 groupuser 中都具有群号消息，
   *    最后返回所有的群组消息（包括群号，群名，群描述）。
   * 2. 然后再根据群组消息里面的群 id，联合 groupuser 和 user 两张表，返回属于该群的所有群成员用户信息：
   *    这里使用 groupser.userid 和 user.id 相等，来返回群成员信息
   * 总结下：首先根据 userid 查出该用户的所有群组，然后根据群组里面的用户 id，再 user 表中根据 id 查出所有的群员信息。
   */
  char sql[1024] = {0};
  /* 1. 查询该用户的所有群组信息，包括群id、群名、群描述 */
  /*  groupuser.groupid == allgroup.id, where 起到一个过滤的作用 */
  sprintf(sql,
          "select a.id, a.groupname, a.groupdesc from allgroup a inner join \
        groupuser b on a.id = b.groupid where b.userid=%d",
          userid);

  std::vector<Group> groupVec;
  MySQL mysql;
  if (mysql.connect()) {
    /* 更新数据库语句 */
    MYSQL_RES *res = mysql.query(sql);  // 指针：内部动态内存开辟，需要释放资源
    if (res != nullptr) {
      // 获取行，根据主键查
      MYSQL_ROW row;
      while ((row = mysql_fetch_row(res)) != nullptr) {
        Group group;
        group.setId(std::atoi(row[0]));
        group.setName(row[1]);
        group.setDesc(row[2]);
        groupVec.push_back(group);
      }
      mysql_free_result(res);
    }
  }

  for (Group &group : groupVec) {
    /* 2. 根据 1. 中查询得到的群组信息，根据群号并联合 groupuser.userid = user.id 获得该群所有用户的详细信息 */
    sprintf(sql,
            "select a.id, a.name, a.state, b.grouprole from user a \
            inner join groupuser b on b.userid = a.id where b.groupid=%d",
            group.getId());
    MYSQL_RES *res = mysql.query(sql);
    if (res != nullptr) {
      // 获取行，根据主键查
      MYSQL_ROW row;
      while ((row = mysql_fetch_row(res)) != nullptr) {
        GroupUser user;
        user.setId(std::atoi(row[0]));
        user.setName(row[1]);
        user.setState(row[2]);
        user.setRole(row[3]);
        group.getUsers().push_back(user);
      } 
      mysql_free_result(res);
    }
  }
  return groupVec;
}

/**
 * 根据指定的 groupid 查询该群组的用户 id 列表，除 userid 自己外，
 * 主要用户群聊业务给群组其它成员群发消息，主要指发送群消息。
 */
std::vector<int> GroupModel::queryGroupUsers(int userid, int groupid) {
  char sql[1024] = {0};
  sprintf(sql,
          "select userid from groupuser where groupid = %d and userid != %d",
          groupid, userid);

  std::vector<int> idVec;
  MySQL mysql;
  if (mysql.connect()) {
    // 更新数据库语句
    MYSQL_RES *res = mysql.query(sql);  // 指针：内部动态内存开辟 需要释放资源
    if (res != nullptr) {
      // 获取行，根据主键查
      MYSQL_ROW row;
      while ((row = mysql_fetch_row(res)) != nullptr) {
        idVec.push_back(std::atoi(row[0]));
      }
      mysql_free_result(res);
    }
  }
  return idVec;
}

#ifndef GROUPMODEL_H
#define GROUPMODEL_H

#include <string>
#include <vector>

#include "group.hpp"

/* 维护群组信息的操作接口方法 */
class GroupModel {
 public:
  /* 创建群组 */
  bool createGroup(Group &group);

  /* 加入群组 */
  void addGroup(int userid, int groupid, std::string role);

  /* 查询用户所在群组信息 */
  std::vector<Group> queryGroups(int userid);

  /**
   * 根据指定的 groupid 查询群组用户 id 列表，除 userid 自己外，
   * 主要用户群聊业务给群组其它成员群发消息
   */
  std::vector<int> queryGroupUsers(int userid, int groupid);
};

#endif

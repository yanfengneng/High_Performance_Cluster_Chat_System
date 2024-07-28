#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "../../thirdparty/json.hpp"
using namespace std;
using json = nlohmann::json;

/* json 序列化示例1 */
string fun1() {
  json js;
  js["msg_type"] = 2;
  js["from"] = "zhang san";
  js["to"] = "li si";
  js["msg"] = "hello, what are you doing now???";

  // json 对象序列化为字符串
  string senBuf = js.dump();

  cout << senBuf.c_str() << endl;

  return senBuf;
}

/* json 序列化示例2 */
string fun2() {
  json js;
  js["id"] = {1, 2, 3, 4, 5};  // 添加数组
  js["name"] = "zhang san";    // 添加key-value
  // 添加对象
  js["msg"]["zhang san"] = "hello world";
  js["msg"]["liu shuo"] = "hello china";
  // 上面等同于下面这句一次性添加数组对象
  js["msg"] = {{"zhang san", "hello world"}, {"liu shuo", "hello china"}};
  // cout << js << endl;
  return js.dump();
}

string fun3() {
  json js;

  // 直接序列化一个vector容器
  vector<int> vec;
  vec.push_back(1);
  vec.push_back(2);
  vec.push_back(5);

  js["list"] = vec;

  // 直接序列化一个map容器
  map<int, string> m;
  m.insert({1, "黄山"});
  m.insert({2, "华山"});
  m.insert({3, "泰山"});

  js["path"] = m;  // 把 map 转换为数组了

  cout << js << endl;
  string sendBuf = js.dump();  // 将 json 对象序列化为 json 字符串
  // cout << sendBuf << endl;
  return sendBuf;
}

int main() {
  string recvBuf = fun3();
  // 字符串反序列化为 json 对象
  json jsbuf = json::parse(recvBuf);
  // cout << jsbuf["msg_type"] << endl;
  // cout << jsbuf["from"] << endl;
  // cout << jsbuf["to"] << endl;
  // cout << jsbuf["msg"] << endl;
  // cout << jsbuf["id"] << endl;
  // // vector<int> arr = jsbuf["id"];
  // auto arr = jsbuf["id"];
  // cout << typeid(arr).name() << endl;
  // cout << arr[2] << endl; 
  // js 对象里面的数组类型，直接放入 vector 容器当中
  vector<int> vec = jsbuf["list"];
  for(int x : vec){
    cout << x << " ";
  }
  cout << endl;

  map<int ,string> mymap = jsbuf["path"];
  for(auto [k,v] : mymap){
    cout << k << " " << v << endl;
  }
  return 0;
}
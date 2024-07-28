# CMake要求的最低版本
cmake_minimum_required(VERSION 3.10)
# 工程名
project(Main)

# 配置编译选项
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -g)

# # 配置头文件搜索路径
# include_directories()
# # 配置库文件搜索路径
# link_directories()

# 设置可执行文件最终存储的路径
# EXECUTABLE_OUTPUT_PATH 可执行文件的输出位置
# PROJECT_SOURCE_DIR 工程目录
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# 设置需要编译的源文件列表
set(SRC_LIST muduo_server.cpp)

# # 把.指定路径下的所有源文件名字放入变量名SRC_LIST里面
# aux_source_directory(. SRC_LIST)

# 生成可执行文件
# 表示生成可执行文件 server，由 SRC_LIST 变量所定义的源文件编译而来
add_executable(server ${SRC_LIST})

# 可执行文件需要链接的库
# 表示 server 需要连接这三个库文件muduo_net、muduo_base、pthread
target_link_libraries(server muduo_net muduo_base pthread)

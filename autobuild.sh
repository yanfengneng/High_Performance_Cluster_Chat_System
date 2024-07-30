# 指定脚本使用 /bin/bash 作为解释器
#!/bin/bash

# 启用调试模式
set -x

# 创建 build 目录并进入
mkdir -p "$(pwd)/build" && cd "$(pwd)/build"

# 运行 cmake 和 make
cmake .. && make

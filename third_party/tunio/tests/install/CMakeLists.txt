# 安装包消费方冒烟工程：仅通过 find_package(tunio) 找到已安装的导出包，
# 在源码树之外独立编译/链接。由 CI 以 -DCMAKE_PREFIX_PATH=<prefix> 配置，
# 用于验证 cmake --install 产物对消费方真正可用（公开头反向依赖的
# detail/ 实现头必须随包导出，见根 CMakeLists.txt 的安装规则）.
cmake_minimum_required(VERSION 3.20)
project(tunio_install_consumer LANGUAGES CXX)

# 导出目标只传递头文件目录，不传递 C++ 标准，消费方自行指定.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(tunio CONFIG REQUIRED)

add_executable(tunio_install_consumer consumer.cpp)
target_link_libraries(tunio_install_consumer PRIVATE tunio::tunio)

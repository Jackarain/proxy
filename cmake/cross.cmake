# 示例：
# ccmake .. -DCOMPILER=mipsel-linux -DCMAKE_SYSTEM_PROCESSOR=mips32 -DCMAKE_TOOLCHAIN_FILE=../cmake/cross.cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -G Ninja -DCMAKE_BUILD_TYPE=Release
# ccmake .. -DCOMPILER=aarch64-linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 -DCMAKE_TOOLCHAIN_FILE=../cmake/cross.cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -G Ninja -DCMAKE_BUILD_TYPE=Release

# 设置目标操作系统
set(CMAKE_SYSTEM_NAME Linux)

# 也可以在这里设置处理器架构
# 一般有 armv7-a|aarch64|mips|arm64|riscv64|riscv32 等值
# set(CMAKE_SYSTEM_PROCESSOR aarch64)

# 设置编译器名称，必须在 PATH 中，一般名称比如：
# mipsel-linux-g++
# mipsel-openwrt-linux-musl-gcc

set(CMAKE_C_COMPILER   ${COMPILER}-gcc)
set(CMAKE_CXX_COMPILER ${COMPILER}-g++)

#set(CMAKE_FIND_ROOT_PATH /usr/bin)
#set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
#set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
#set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)


# MinGW-w64 x86_64 交叉编译工具链
# Usage: cmake -B build-mingw -S . -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-toolchain.cmake [-DUSE_WINTUN_DRIVER=ON]

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_C_COMPILER   x86_64-w64-mingw32-gcc)

# 必须指定 sysroot，否则 mingw-w64 会找到宿主机 Boost 头文件，导致签名不匹配
set(CMAKE_SYSROOT /usr/x86_64-w64-mingw32)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

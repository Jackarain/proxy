# the name of the target operating system
set(CMAKE_SYSTEM_NAME Linux)

#set(CMAKE_SYSTEM_PROCESSOR mips32)
#set(CMAKE_SYSTEM_PROCESSOR arm64)

# see https://toolchains.bootlin.com/
# which compilers to use for C and C++
# mipsel-linux-g++
# mipsel-openwrt-linux-musl-gcc
# ...

set(CMAKE_C_COMPILER   ${COMPILER}-gcc)
set(CMAKE_CXX_COMPILER ${COMPILER}-g++)

# where is the target environment located
#set(CMAKE_FIND_ROOT_PATH /usr/bin)

# adjust the default behavior of the FIND_XXX() commands:
# search programs in the host environment
#set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# search headers and libraries in the target environment
#set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
#set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)


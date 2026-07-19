// xxhash.cpp
// 编译 xxhash 库到单独的翻译单元，避免 XXH_INLINE_ALL 导致的编译膨胀
// 此文件定义 XXH_IMPLEMENTATION 以在一个翻译单元中实例化所有 xxhash 函数，
// 其他翻译单元仅包含声明和类型定义（通过 XXH_STATIC_LINKING_ONLY）。
#define XXH_IMPLEMENTATION
#include "proxy/xxhash.hpp"

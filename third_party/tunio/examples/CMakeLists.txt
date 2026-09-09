# 示例按语言分目录：cpp/ 下的 C++ 示例经 CMake 构建（可执行文件统一
# 输出到构建目录的 bin/）；python/ 下的 Python 示例为直接运行脚本，配合
# bindings/python 的 ctypes 绑定使用，无需编译。c/ 下的 C 示例链接
# tunio_c（C API 共享库），仅在启用 C/Python 绑定构建时加入。
add_subdirectory(cpp)
if(TUNIO_BUILD_C OR TUNIO_BUILD_PYTHON)
  add_subdirectory(c)
endif()

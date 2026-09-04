# tunio 的 C 绑定：tunio_c 共享库（c_api.h 中的同一套句柄 API，阻塞调用与
# 回调风格异步变体共存，实现于 src/c_api.cpp）。Python 绑定通过 ctypes
# 调用它，无需绑定语言相关的编译依赖；TUNIO_BUILD_C 单独开启时也可独立构建。

# 共享库需要 PIC 目标文件：静态库 tunio 亦被链接进 tunio_c.
set_target_properties(tunio PROPERTIES POSITION_INDEPENDENT_CODE ON)

find_package(Threads REQUIRED)

add_library(tunio_c SHARED src/c_api.cpp)
target_include_directories(tunio_c PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
# tunio 的 BUILD_INTERFACE 提供引擎公开头与实现头所在目录（include/、src/）.
target_link_libraries(tunio_c PRIVATE tunio Threads::Threads)

if(WIN32)
  # Windows 平台 tunio 静态库内部实现所需系统库（与根 CMakeLists 的
  # tunio 链接保持一致；Wintun 编译定义由 tunio 的 PUBLIC 定义传播）。
  if(USE_WINTUN_DRIVER)
    target_link_libraries(tunio_c PRIVATE iphlpapi cfgmgr32 setupapi ws2_32)
  else()
    target_link_libraries(tunio_c PRIVATE ws2_32 iphlpapi)
  endif()
  set_target_properties(tunio_c PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

if(NOT MSVC)
  target_compile_options(tunio_c PRIVATE -Wall -Wextra)
endif()

# 随主库安装/导出时附带 C 绑定头文件（产物 tunio_c 不随 find_package 导出）。
if(TUNIO_INSTALL)
  install(DIRECTORY include/tunio
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )
endif()

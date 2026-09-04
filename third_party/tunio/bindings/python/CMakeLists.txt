# tunio 的 Python 绑定目录：仅把 bindings/c 构建的 tunio_c 共享库产物
# 调整到本包目录（bindings/python/tunio/），使源码树内可直接 import tunio。
set_target_properties(tunio_c PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tunio
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tunio
  ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tunio
)

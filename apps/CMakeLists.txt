if (ANDROID)
    # Android 场景只构建 SWIG 库 (libxproxy.so), 不构建桌面端可执行文件.
    # 查找 Android log 库 (libproxy 的 xlogger 输出使用 __android_log_print).
    find_library(
        log-lib
        log
    )
    set(LOG_LIBRARY ${log-lib})
    link_libraries("${log-lib}")
    # SWIG 输出目录: 生成的 Java 包装文件输出到构建目录 swig/.
    set(CMAKE_SWIG_OUTDIR ${CMAKE_BINARY_DIR}/swig)
    find_package(SWIG)
    add_subdirectory(android)
else()
    add_subdirectory(httpc)
    add_subdirectory(proxy_server)
    add_subdirectory(launcher)
endif()

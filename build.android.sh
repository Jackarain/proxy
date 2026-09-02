#!/bin/bash
# arm64-v8a   armeabi-v7a   x86         x86_64
#
# 使用方法:
# ./build.android.sh <path-to-src> <path-to-ndk> [host-tag] [architecture]
# 示例:
# ./build.android.sh ~/Documents/proxy /Users/jack/Library/Android/sdk/ndk/26.1.10909125
# ./build.android.sh ~/Documents/proxy /Users/jack/Library/Android/sdk/ndk/26.1.10909125 darwin-x86_64 arm64-v8a
# ./build.android.sh /root/proxy /root/ndk linux-x86_64 armeabi-v7a
#

SRC_PATH=$1
NDK_PATH=$2
HOST_TAG=${3:-windows-x86_64}
TARGET_ARCH=$4
BUILD_TYPE="Release"

# 如果指定了具体的架构则只编译该架构，否则默认编译所有架构
if [ -n "$TARGET_ARCH" ]; then
    ARCHITECTURES=("$TARGET_ARCH")
else
    ARCHITECTURES=("arm64-v8a" "armeabi-v7a" "x86" "x86_64")
fi

kernel=$(uname -s)

if [ "$kernel" = "Linux" ]; then
    HOST_TAG=linux-x86_64
elif [ "$kernel" = "Darwin" ]; then
    HOST_TAG=darwin-x86_64
elif [ "$kernel" = "MINGW64_NT-10.0" ]; then
    HOST_TAG=windows-x86_64
fi

echo "SRC_PATH: ${SRC_PATH}"
echo "NDK_PATH: ${NDK_PATH}"
echo "HOST_TAG: ${HOST_TAG}"
echo "ARCHITECTURES: ${ARCHITECTURES[*]}"

for ARCH in "${ARCHITECTURES[@]}"
do
    cmake -S ${SRC_PATH} -B android/$ARCH -DCMAKE_TOOLCHAIN_FILE=${NDK_PATH}/build/cmake/android.toolchain.cmake -DANDROID_ABI=${ARCH} -DANDROID_PLATFORM=android-19 -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DENABLE_USE_OPENSSL=OFF -DENABLE_USE_BORINGSSL=ON -G Ninja
    cmake --build android/$ARCH
    mkdir -p release/$ARCH
    # 桌面端可执行文件 (非 Android 场景).
    if ls android/$ARCH/bin/* >/dev/null 2>&1; then
        ${NDK_PATH}/toolchains/llvm/prebuilt/${HOST_TAG}/bin/llvm-strip android/$ARCH/bin/*
        cp android/$ARCH/bin/* release/$ARCH/
    fi
    # SWIG 库 (libxproxy.so) 与生成的 Java 包装文件.
    ${NDK_PATH}/toolchains/llvm/prebuilt/${HOST_TAG}/bin/llvm-strip android/$ARCH/lib/libxproxy.so
    cp android/$ARCH/lib/libxproxy.so release/$ARCH/
    # 同步 libxproxy.so 到 Android 客户端工程 (Flutter), 避免 APK 编译时缺少 .so.
    JNI_LIBS_DIR=${SRC_PATH}/apps/android/xproxy/android/app/src/main/jniLibs/${ARCH}
    if [ -d "${SRC_PATH}/apps/android/xproxy/android/app" ]; then
        mkdir -p ${JNI_LIBS_DIR}
        cp android/$ARCH/lib/libxproxy.so ${JNI_LIBS_DIR}/libxproxy.so
        echo "copied libxproxy.so -> ${JNI_LIBS_DIR}/libxproxy.so"
    fi
    mkdir -p outputs/binaries
    if [ -d "android/$ARCH/swig" ]; then
        cp -r android/$ARCH/swig/* outputs/
    fi
    cp -r release/* outputs/binaries/
done

echo "Build finished."

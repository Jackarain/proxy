#!/bin/bash
# arm64-v8a   armeabi-v7a    x86         x86_64
#
# 使用方法:
#
# ./build.android.sh <path-to-src> <path-to-ndk> <host-tag>
# 示例:
# ./build.android.sh ~/Documents/proxy /Users/jack/Library/Android/sdk/ndk/26.1.10909125
# ./build.android.sh ~/Documents/proxy /Users/jack/Library/Android/sdk/ndk/26.1.10909125 darwin-x86_64
# ./build.android.sh /root/proxy /root/ndk linux-x86_64
# ./build.android.sh ~/proxy ~/ndk windows-x86_64
#

ARCHITECTURES=("arm64-v8a" "armeabi-v7a" "x86" "x86_64")

SRC_PATH=$1
NDK_PATH=$2
HOST_TAG=${3:-windows-x86_64}
BUILD_TYPE="Release"

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

for ARCH in "${ARCHITECTURES[@]}"
do
    cmake -S ${SRC_PATH} -B android/$ARCH -DCMAKE_TOOLCHAIN_FILE=${NDK_PATH}/build/cmake/android.toolchain.cmake -DANDROID_ABI=${ARCH} -DANDROID_PLATFORM=android-19 -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -G Ninja
    cmake --build android/$ARCH
    ${NDK_PATH}/toolchains/llvm/prebuilt/${HOST_TAG}/bin/llvm-strip android/$ARCH/bin/*
    mkdir -p release/$ARCH
    cp android/$ARCH/bin/* release/$ARCH/
    mkdir -p outputs/binaries
    cp -r release/* outputs/binaries/
done

echo "Build finished."

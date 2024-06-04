#!/bin/bash
# arm64-v8a   armeabi-v7a    x86         x86_64
#
# 使用方法:
#
# ./build.sh <path-to-csa> <path-to-ndk> <host-tag>
# 示例:
# ./build.sh ~/Documents/proxy /Users/jack/Library/Android/sdk/ndk/26.1.10909125
# ./build.sh ~/Documents/proxy /Users/jack/Library/Android/sdk/ndk/26.1.10909125 darwin-x86_64
# ./build.sh /root/proxy /root/ndk linux-x86_64
# ./build.sh ~/proxy ~/ndk windows-x86_64
#

ARCHITECTURES=("arm64-v8a" "armeabi-v7a" "x86" "x86_64")

CSA_PATH=$1
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

echo "CSA_PATH: ${CSA_PATH}"
echo "NDK_PATH: ${NDK_PATH}"
echo "HOST_TAG: ${HOST_TAG}"

for ARCH in "${ARCHITECTURES[@]}"
do
    cmake -S ${CSA_PATH} -B android/$ARCH -DCMAKE_TOOLCHAIN_FILE=${NDK_PATH}/build/cmake/android.toolchain.cmake -DANDROID_ABI=${ARCH} -DANDROID_PLATFORM=android-19 -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -G Ninja
    cmake --build android/$ARCH
    mkdir -p release/$ARCH
    cp android/$ARCH/bin/* release/$ARCH/
done

echo "Build finished."

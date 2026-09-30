#!/bin/bash
# 用法:
# ./build.ohos.sh <path-to-src> [arch] [sdk-path] [command-line-tools]
# 示例:
# ./build.ohos.sh /root/proxy arm64-v8a /opt/ohos/command-line-tools/sdk /opt/ohos/command-line-tools
#
# 1) 用 OHOS NDK 工具链编译 libxproxy.so;
# 2) 同步到 apps/ohos/xproxy/entry/libs/<arch>/;
# 3) 若提供 command-line-tools 则继续构建 HAP.

set -e

SRC_PATH=${1:-$(pwd)}
TARGET_ARCH=${2:-arm64-v8a}
SDK_PATH=${3:-/opt/ohos/command-line-tools/sdk}
TOOLS_PATH=${4:-/opt/ohos/command-line-tools}
BUILD_TYPE=Release

NATIVE_PATH=${SDK_PATH}/default/openharmony/native
TOOLCHAIN=${NATIVE_PATH}/build/cmake/ohos.toolchain.cmake
STRIP=${NATIVE_PATH}/llvm/bin/llvm-strip
BUILD_DIR=${SRC_PATH}/ohos/${TARGET_ARCH}

if [ ! -f "${TOOLCHAIN}" ]; then
    echo "找不到 OHOS 工具链: ${TOOLCHAIN}" >&2
    exit 1
fi

echo "SRC_PATH: ${SRC_PATH}"
echo "TARGET_ARCH: ${TARGET_ARCH}"
echo "SDK_PATH: ${SDK_PATH}"

cmake -S ${SRC_PATH} -B ${BUILD_DIR} -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN} \
    -DOHOS_ARCH=${TARGET_ARCH} \
    -DOHOS_PLATFORM_LEVEL=18 \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    -DENABLE_USE_OPENSSL=OFF -DENABLE_USE_BORINGSSL=ON -DENABLE_USE_WOLFSSL=OFF \
    -DENABLE_BUILD_WERROR=OFF -DENABLE_BUILD_EXAMPLES=OFF
cmake --build ${BUILD_DIR} --target xproxy

mkdir -p release/${TARGET_ARCH}
${STRIP} ${BUILD_DIR}/lib/libxproxy.so
cp ${BUILD_DIR}/lib/libxproxy.so release/${TARGET_ARCH}/

JNI_LIBS_DIR=${SRC_PATH}/apps/ohos/xproxy/entry/libs/${TARGET_ARCH}
if [ -d "${SRC_PATH}/apps/ohos/xproxy" ]; then
    mkdir -p ${JNI_LIBS_DIR}
    cp ${BUILD_DIR}/lib/libxproxy.so ${JNI_LIBS_DIR}/libxproxy.so
    echo "copied libxproxy.so -> ${JNI_LIBS_DIR}/libxproxy.so"
fi

if [ -x "${TOOLS_PATH}/bin/hvigorw" ]; then
    export DEVECO_SDK_HOME=${SDK_PATH}
    export DEVECO_NODE_HOME=${TOOLS_PATH}/tool/node
    export PATH=${DEVECO_NODE_HOME}/bin:$PATH
    cd ${SRC_PATH}/apps/ohos/xproxy
    ${TOOLS_PATH}/bin/hvigorw assembleHap --mode module -p product=default -p buildMode=release --no-daemon
    cd ${SRC_PATH}
    cp apps/ohos/xproxy/entry/build/default/outputs/default/entry-default-unsigned.hap \
        release/${TARGET_ARCH}/xproxy-${TARGET_ARCH}.hap
    echo "hap -> release/${TARGET_ARCH}/xproxy-${TARGET_ARCH}.hap"
fi

echo "Build finished."

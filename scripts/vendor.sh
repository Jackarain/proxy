#!/usr/bin/env bash
#
# 重新拉取 third_party 中的三方库：boringssl/tunio/tinyrpc 跟随默认分支最新提交，
# 其余固定到指定 tag/分支/commit。
#
# 各库来源与处理方式：
#   boost      官方 superproject（含子模块），用 scripts/prune_boost.py 裁剪出用到的库，
#              再套用 scripts/patch/boost 下的定制补丁
#   boringssl  官方仓库，默认分支（main）最新提交
#   fmt        仅刷新 include/ 与 src/，保留仓库内定制 CMakeLists.txt
#   httpc      jackarain/httpc，固定分支（适配提交后建议改为固定 commit）
#   openssl    官方仓库 master，克隆后在临时目录运行 scripts/gen_cmake.py 生成精简工程
#   snmalloc   官方仓库，固定 tag
#   tinyrpc    jackarain/tinyrpc，默认分支（master）最新提交；仅保留 include/
#              （稀疏检出，避免拉取其内嵌 third_party/）
#   tunio      jackarain/tunio，默认分支（master）最新提交
#   wintun     wintun.net 发行包 0.14.1；ring_buffer.h 取自 OpenVPN v2.5.10
#   wolfssl    官方仓库，固定 commit，并应用 scripts/patch/wolfssl 下的补丁
#   zlib       官方仓库，固定 tag
#
# 用法: scripts/vendor.sh <库名 ... | all>
#   需显式指定要更新的库（可用 all 更新全部）；不带参数或 -h/--help 时仅打印帮助。
#
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
third_party="${root_dir}/third_party"
patch_dir="${root_dir}/scripts/patch"
tmp="$(mktemp -d)"

cleanup() {
    rm -rf "${tmp}"
}
trap cleanup EXIT

# 打印已更新的提交，便于记录本次拉取到的版本。
show_head() {
    echo "    commit: $(git -C "$1" rev-parse HEAD)"
}

# 克隆到临时目录，ref 可为 tag、分支、commit，或 latest（默认分支最新提交）。
fetch_repo() {
    local name="$1" repo="$2" ref="$3"
    echo ">>> ${name} (${ref})"
    rm -rf "${tmp:?}/${name}"
    if [ "${ref}" = "latest" ]; then
        git clone --quiet --depth 1 "${repo}" "${tmp}/${name}"
        show_head "${tmp}/${name}"
        return
    fi
    if ! git clone --quiet --depth 1 --branch "${ref}" \
        "${repo}" "${tmp}/${name}" 2>/dev/null; then
        rm -rf "${tmp:?}/${name}"
        git init --quiet "${tmp}/${name}"
        git -C "${tmp}/${name}" remote add origin "${repo}"
        git -C "${tmp}/${name}" fetch --quiet --depth 1 origin "${ref}"
        git -C "${tmp}/${name}" checkout --quiet FETCH_HEAD
    fi
}

# 克隆后整体替换 third_party/<name>；额外参数为要剔除的路径。
clone_lib() {
    local name="$1" repo="$2" ref="$3"; shift 3
    fetch_repo "${name}" "${repo}" "${ref}"
    local src="${tmp}/${name}" path
    for path in "$@"; do
        rm -rf "${src:?}/${path}"
    done
    rm -rf "${third_party:?}/${name}"
    rm -rf "${src}/.git"
    mv "${src}" "${third_party}/${name}"
}

# 套用 scripts/patch/<name>/ 下的全部补丁（补丁路径以仓库根为基准）。
apply_patches() {
    local name="$1"
    local dir="${patch_dir}/${name}" p
    [ -d "${dir}" ] || return 0
    for p in "${dir}"/*.patch; do
        [ -e "${p}" ] || continue
        git -C "${root_dir}" apply "${p}"
    done
}

# boost 本工程直接使用的库：其余库及 test/example/doc 等目录会被 prune_boost.py 裁掉，
# 传递依赖（如 align、smart_ptr 等）由该脚本按 Boost 的 CMake 依赖规则自动补齐。
BOOST_KEEP_LIBS=(
    asio assert beast chrono container core date_time exception filesystem
    functional json nowide optional program_options scope signals2 system
    thread unordered url uuid variant2
)

# boost：superproject 含子模块，拉取后裁剪出用到的库，再套用 scripts/patch/boost 下的定制补丁。
clone_boost() {
    local ref="$1"
    echo ">>> boost (${ref})"
    rm -rf "${tmp:?}/boost"
    git clone --quiet --depth 1 --branch "${ref}" \
        --recurse-submodules --shallow-submodules --jobs 8 \
        https://github.com/boostorg/boost.git "${tmp}/boost"
    find "${tmp}/boost" -name .git -prune -exec rm -rf {} +
    rm -rf "${third_party:?}/boost"
    mv "${tmp}/boost" "${third_party}/boost"
    python3 "${root_dir}/scripts/prune_boost.py" \
        "${third_party}/boost" "${BOOST_KEEP_LIBS[@]}"
    apply_patches boost
}

# openssl：克隆官方仓库，在其内运行 gen_cmake.py 生成精简工程后导出。
clone_openssl() {
    local ref="$1"
    echo ">>> openssl (${ref})"
    rm -rf "${tmp:?}/openssl"
    git clone --quiet --depth 1 --branch "${ref}" \
        https://github.com/openssl/openssl.git "${tmp}/openssl"
    cp "${root_dir}/scripts/gen_cmake.py" "${tmp}/openssl/gen_cmake.py"
    rm -rf "${third_party:?}/openssl"
    ( cd "${tmp}/openssl" && python3 gen_cmake.py -o "${third_party}/openssl" )
}

# fmt：保留仓库内定制 CMakeLists.txt，仅刷新上游源码。
update_fmt() {
    fetch_repo fmt https://github.com/fmtlib/fmt.git 12.1.0
    rm -rf "${third_party:?}/fmt/include" "${third_party:?}/fmt/src"
    mkdir -p "${third_party}/fmt"
    cp -a "${tmp}/fmt/include" "${tmp}/fmt/src" "${third_party}/fmt/"
}

# tinyrpc：仅保留 include/；仓库内嵌 third_party/ 体积过大，用稀疏检出避免拉取。
# tinyrpc：只保留 include/（稀疏检出），取默认分支最新提交。
update_tinyrpc() {
    local repo="https://github.com/Jackarain/tinyrpc.git"
    local src="${tmp}/tinyrpc"
    echo ">>> tinyrpc (latest)"
    rm -rf "${src}"
    git init --quiet "${src}"
    git -C "${src}" remote add origin "${repo}"
    git -C "${src}" sparse-checkout set --no-cone '/include/'
    git -C "${src}" fetch --quiet --depth 1 --filter=blob:none origin HEAD
    git -C "${src}" checkout --quiet FETCH_HEAD
    show_head "${src}"
    rm -rf "${third_party:?}/tinyrpc"
    mkdir -p "${third_party}/tinyrpc"
    cp -a "${src}/include" "${third_party}/tinyrpc/"
}

# wintun：下载发行包，另从 OpenVPN 取 ring_buffer.h（归一化为 UTF-8 BOM + CRLF）。
update_wintun() {
    local ver="0.14.1"
    echo ">>> wintun (${ver})"
    curl -fsSL -o "${tmp}/wintun.zip" \
        "https://www.wintun.net/builds/wintun-${ver}.zip"
    rm -rf "${tmp:?}/wintun"
    unzip -q "${tmp}/wintun.zip" -d "${tmp}/wintun"
    rm -rf "${third_party:?}/wintun"
    cp -a "${tmp}/wintun/wintun" "${third_party}/wintun"
    curl -fsSL -o "${tmp}/ring_buffer.h" \
        https://raw.githubusercontent.com/OpenVPN/openvpn/v2.5.10/src/openvpn/ring_buffer.h
    { printf '\xEF\xBB\xBF'; sed -e 's/$/\r/' "${tmp}/ring_buffer.h"; } \
        > "${third_party}/wintun/include/ring_buffer.h"
}

# wolfssl：固定上游 commit，并移除其安装规则（作为 third_party 由宿主工程接管安装）。
update_wolfssl() {
    clone_lib wolfssl https://github.com/wolfSSL/wolfssl.git \
        5297cc74b4ec1fa52eac153e230d5b9e14b6e95a
    apply_patches wolfssl
}

# 全部库名，同时作为不带参数时的更新顺序。
ALL_LIBS=(boost boringssl fmt httpc openssl snmalloc tinyrpc tunio wintun wolfssl zlib)

# 按库名分发到对应的更新逻辑。
update_lib() {
    case "$1" in
        boost)
            clone_boost boost-1.92.0 ;;
        boringssl)
            clone_lib boringssl https://github.com/google/boringssl.git latest ;;
        fmt)
            update_fmt ;;
        httpc)
            clone_lib httpc https://github.com/Jackarain/httpc.git master .github .gitignore ;;
        openssl)
            clone_openssl master ;;
        snmalloc)
            clone_lib snmalloc https://github.com/microsoft/snmalloc.git 0.7.3 ;;
        tinyrpc)
            update_tinyrpc ;;
        tunio)
            clone_lib tunio https://github.com/Jackarain/tunio.git latest ;;
        wintun)
            update_wintun ;;
        wolfssl)
            update_wolfssl ;;
        zlib)
            clone_lib zlib https://github.com/madler/zlib.git v1.3.2 .github .gitignore ;;
        *)
            echo "未知库: $1" >&2
            echo "可用库: ${ALL_LIBS[*]}" >&2
            return 1 ;;
    esac
}

usage() {
    cat <<EOF
用法: scripts/vendor.sh <库名 ... | all>

必须显式指定要更新的库：可以是一个或多个库名，或用 all 更新全部。
不带参数或使用 -h / --help 时仅打印本帮助。

可用库名:
  ${ALL_LIBS[*]}

示例:
  scripts/vendor.sh all           # 更新全部
  scripts/vendor.sh openssl       # 仅更新 openssl
  scripts/vendor.sh boost zlib    # 更新 boost 与 zlib
EOF
}

if [ "$#" -eq 0 ] || [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
    usage
    exit 0
fi

# 展开 all 为全部库名。
expanded=()
for lib in "$@"; do
    if [ "${lib}" = "all" ]; then
        expanded+=("${ALL_LIBS[@]}")
    else
        expanded+=("${lib}")
    fi
done
set -- "${expanded[@]}"

# 先校验全部库名，避免参数拼错时只执行了一部分。
for lib in "$@"; do
    case " ${ALL_LIBS[*]} " in
        *" ${lib} "*) ;;
        *)
            echo "未知库: ${lib}" >&2
            echo "可用库: ${ALL_LIBS[*]}" >&2
            exit 1 ;;
    esac
done

mkdir -p "${third_party}"
for lib in "$@"; do
    update_lib "${lib}"
done

echo "done"

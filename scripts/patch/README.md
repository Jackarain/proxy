# third_party 补丁说明

`scripts/vendor.sh` 按固定版本重新拉取 `third_party/` 下的三方库后，会自动套用本目录中
与各库同名的子目录下的所有补丁（补丁路径以仓库根为基准，使用 `git apply` 应用）。这些
补丁用于让上游源码适配本项目的构建方式与需求，不能直接丢弃，更新三方库版本后需要相应维护。

## 目录结构

```
scripts/patch/
├── boost/
│   ├── 0001-Add-option-BOOST_ASIO_BUILD_CONTEXT.patch
│   ├── 0001-Enable-UTF-8-paths-with-BOOST_FILESYSTEM_USE_UTF8_CO.patch
│   └── 0001-Resolve-mismatched-tags-compiler-warning.patch
├── wolfssl/
│   └── remove-install.patch
└── README.md
```

## boost/

### 0001-Add-option-BOOST_ASIO_BUILD_CONTEXT.patch

- **作用对象**：`third_party/boost/libs/asio/CMakeLists.txt`
- **修改内容**：
  - 新增 CMake 选项 `BOOST_ASIO_BUILD_CONTEXT`，默认 `ON`；
  - 该选项为 `OFF` 时不再构建、链接与导出 `boost_asio_spawn`。

**为什么**：Boost.Asio 的 spawn（栈式协程）依赖 Boost.Context，原版 `CMakeLists.txt` 中
`boost_asio_spawn` 与 `boost_asio` 无条件链接 `Boost::context`。本项目不使用
spawn / Boost.Context（协程基于 C++20 协程），并在根 `CMakeLists.txt` 中通过
`set(BOOST_ASIO_BUILD_CONTEXT OFF)` 与 `BOOST_EXCLUDE_LIBRARIES` 排除 `context`。
此时 `Boost::context` 目标不存在，若 asio 仍无条件链接会导致 CMake 配置阶段失败。本补丁
把 spawn 的构建与链接收口到该选项下，关闭后即可在排除 Boost.Context 的情况下正常构建 Asio。

### 0001-Enable-UTF-8-paths-with-BOOST_FILESYSTEM_USE_UTF8_CO.patch

- **作用对象**：`third_party/boost/libs/filesystem/src/path.cpp`
- **修改内容**：在启用 UTF-8 codecvt facet 的预处理器条件中追加
  `|| defined(BOOST_FILESYSTEM_USE_UTF8_CODECVT_FACET)`。

**为什么**：本项目使用 `boost::filesystem` 处理路径，并定义
`BOOST_FILESYSTEM_USE_UTF8_CODECVT_FACET` 要求统一使用 UTF-8 编码。但 Boost.Filesystem
原版只在 Apple / BSD 系 / Solaris / Haiku 等系统上启用该 facet，其它平台会忽略该宏，路径
编码退化为 locale 相关编码（如 GBK 下中文路径乱码）。本补丁使任意平台在定义该宏后都强制
使用 UTF-8 编码路径。

### 0001-Resolve-mismatched-tags-compiler-warning.patch

- **作用对象**：`third_party/boost/libs/asio/include/boost/asio/detail/io_uring_service.hpp`
- **修改内容**：把前置声明 `class io_object;` 改为 `struct io_object;`。

**为什么**：`io_object` 实际定义为 `struct`，前置声明写成 `class` 会触发 Clang 的
`-Wmismatched-tags` 告警。

## wolfssl/

### remove-install.patch

- **作用对象**：`third_party/wolfssl/CMakeLists.txt`
- **修改内容**：删除 wolfssl 的 `install()` 规则（库、头文件、pkgconfig、导出集等）。

**为什么**：wolfssl 作为 `third_party` 由 `add_subdirectory` 引入时，不应向宿主工程的安装
目标注入自身的安装规则（否则安装宿主工程会连带安装 wolfssl）。移除后由宿主工程自行决定
安装内容。

## 使用方式

`scripts/vendor.sh` 在拉取对应库后会自动套用，无需手工执行。脚本必须显式指定要更新的库（多个库名或 `all`），只有
被更新到的库才会套用其补丁；不带参数或传 `-h` / `--help` 时仅打印用法帮助：

```bash
scripts/vendor.sh            # 不带参数只打印帮助
scripts/vendor.sh all        # 更新全部库
scripts/vendor.sh boost      # 仅更新 boost，自动套用 scripts/patch/boost/ 下的补丁
scripts/vendor.sh boost zlib # 更新指定的多个库
scripts/vendor.sh wolfssl    # 仅更新 wolfssl，自动套用 scripts/patch/wolfssl/ 下的补丁
```

如需手工应用（在仓库根目录执行）：

```bash
# 先校验是否可干净应用
git apply --check scripts/patch/boost/0001-Add-option-BOOST_ASIO_BUILD_CONTEXT.patch
# 再应用
git apply scripts/patch/boost/0001-Add-option-BOOST_ASIO_BUILD_CONTEXT.patch
```

## 维护说明

- 补丁路径均以仓库根为基准（`-p1`），因此可在仓库根统一用 `git apply` 应用。
- 更新三方库版本后，若补丁无法干净应用，需按上述各补丁的意图手工同步修改，并更新补丁文件。
- 新增针对某个库的补丁：放到 `scripts/patch/<库名>/` 下即可，`vendor.sh` 会自动套用。
- `vendor.sh` 拉取 boost 后按 `BOOST_KEEP_LIBS`（在 `scripts/vendor.sh` 中）裁剪目录树，
  只保留用到的库（含传递依赖，由 `scripts/prune_boost.py` 计算）。本工程新用到某个 boost
  库时，需要把库名补进 `BOOST_KEEP_LIBS`，否则该库不会出现在 `third_party/boost/libs` 下。

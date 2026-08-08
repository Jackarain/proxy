# Boost 补丁说明

本目录存放针对 `third_party/boost/` 的定制补丁，用于解决本项目的两个特定需求：

1. 在**不构建 boost.context** 的前提下正常使用 Boost.Asio；
2. 在所有平台上统一以 **UTF-8 编码** 处理文件系统路径。

补丁文件为 `git format-patch` 格式（包含 `From` / `Subject` 头），可直接用 `git apply` / `git am` 应用。

---

## 补丁一：`0001-Add-option-BOOST_ASIO_BUILD_CONTEXT.patch`

- **作用对象**：`third_party/boost/libs/asio/CMakeLists.txt`
- **修改内容**：
  - 新增 CMake 选项 `BOOST_ASIO_BUILD_CONTEXT`，默认 `ON`；
  - 当该选项为 `OFF` 时，不再构建 `boost_asio_spawn`（spawn 栈式协程）目标，`boost_asio` 与安装目标也不再链接/导出它。

### 为什么做这个修改

Boost.Asio 的 spawn（栈式协程）支持依赖 Boost.Context。原版 `CMakeLists.txt` 中：

- `boost_asio_spawn` 目标**无条件** `target_link_libraries(... Boost::context)`；
- `boost_asio` 目标又**无条件**链接 `Boost::asio_spawn`。

而本项目并不使用 spawn / Boost.Context——协程部分基于 C++20 协程（`net::awaitable`）。为了精简依赖，根 `CMakeLists.txt` 做了两件事：

```cmake
set(BOOST_ASIO_BUILD_CONTEXT OFF)
set(BOOST_EXCLUDE_LIBRARIES
    ...
    "context"    # 上下文切换
    ...
)
```

问题在于：`context` 被 `BOOST_EXCLUDE_LIBRARIES` 排除后，`Boost::context` 这个 CMake 目标**根本不存在**，而 asio 仍无条件链接它，导致 CMake 配置阶段直接报错（`target not found`）。

本补丁将 spawn 的构建与链接收口到 `BOOST_ASIO_BUILD_CONTEXT` 选项下：

- 默认 `ON`，保持 Boost.Asio 原有行为不变；
- 项目通过 `set(BOOST_ASIO_BUILD_CONTEXT OFF)`（先于 `add_subdirectory(third_party/boost)`）关闭后，`Boost::asio_spawn` 及对 `Boost::context` 的依赖被一并移除，从而可以在排除 Boost.Context 的情况下正常构建 Asio。

### 如何应用

在仓库根目录执行：

```bash
# 方式一：仅应用，不产生提交（推荐）
git apply doc/patch/0001-Add-option-BOOST_ASIO_BUILD_CONTEXT.patch

# 方式二：应用并创建提交（保留补丁的提交信息）
git am doc/patch/0001-Add-option-BOOST_ASIO_BUILD_CONTEXT.patch

# 方式三：通用 patch 工具
patch -p1 < doc/patch/0001-Add-option-BOOST_ASIO_BUILD_CONTEXT.patch
```

> 提示：应用前可用 `git apply --check doc/patch/xxx.patch` 校验补丁是否可干净应用；若已应用过，`git apply` 会报错提示补丁已存在。

---

## 补丁二：`0001-Enable-UTF-8-paths-with-BOOST_FILESYSTEM_USE_UTF8_CO.patch`

- **作用对象**：`third_party/boost/libs/filesystem/src/path.cpp`
- **修改内容**：在启用 UTF-8 codecvt facet 的预处理器条件中追加 `|| defined(BOOST_FILESYSTEM_USE_UTF8_CODECVT_FACET)`。

### 为什么做这个修改

本项目在根 `CMakeLists.txt` 中：

```cmake
# 默认使用 boost.filesystem 而不是 std.filesystem, 这是因为
# std.filesystem 在 GCC-12.3 以下的版本在非 ASCII 中会构造失败
# 参考链接: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=95048
add_definitions(-DUSE_BOOST_FILESYSTEM)
...
add_definitions(-DBOOST_FILESYSTEM_USE_UTF8_CODECVT_FACET)
```

即：项目用 `boost::filesystem` 处理路径（尤其是中文等非 ASCII 路径），并通过 `BOOST_FILESYSTEM_USE_UTF8_CODECVT_FACET` 宏要求统一使用 UTF-8 编码。

但 Boost.Filesystem 原版只在 **Apple / BSD 系 / Solaris / Haiku** 等系统上启用 UTF-8 codecvt facet（这些系统的系统调用本就要求 UTF-8 编码）。在其它平台（Linux、Windows 等），即便定义了 `BOOST_FILESYSTEM_USE_UTF8_CODECVT_FACET`，该宏也会被原样忽略，路径编码退化为 locale 相关编码——在非 UTF-8 locale（如 GBK）下中文路径会乱码。

本补丁把 `BOOST_FILESYSTEM_USE_UTF8_CODECVT_FACET` 纳入启用条件，使定义该宏后**任意平台**都强制使用 UTF-8 编码的路径，从而保证：

- Linux 非 UTF-8 locale（如 `zh_CN.GBK`）下也能正确处理中文路径；
- Windows 等其它平台行为一致，避免路径乱码。

### 如何应用

在仓库根目录执行：

```bash
# 方式一：仅应用，不产生提交（推荐）
git apply doc/patch/0001-Enable-UTF-8-paths-with-BOOST_FILESYSTEM_USE_UTF8_CO.patch

# 方式二：应用并创建提交
git am doc/patch/0001-Enable-UTF-8-paths-with-BOOST_FILESYSTEM_USE_UTF8_CO.patch

# 方式三：通用 patch 工具
patch -p1 < doc/patch/0001-Enable-UTF-8-paths-with-BOOST_FILESYSTEM_USE_UTF8_CO.patch
```

---

## 附：维护说明

- 两个补丁都以 `third_party/boost/` 下的源码为基准，路径从仓库根目录计算（`-p1` 级别）。
- 更新第三方 Boost 版本后，若补丁未能自动合并（`git apply` 报冲突），需按上述"为什么"部分的意图手工同步修改。
- 应用补丁后，如需验证：补丁一可检查 `third_party/boost/libs/asio/CMakeLists.txt` 中是否存在 `option(BOOST_ASIO_BUILD_CONTEXT ...)`；补丁二可检查 `path.cpp` 中启用 UTF-8 facet 的条件是否包含 `BOOST_FILESYSTEM_USE_UTF8_CODECVT_FACET`。

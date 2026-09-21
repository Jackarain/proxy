# httpc

基于 [Boost.Asio](https://www.boost.org/doc/libs/release/doc/html/boost_asio.html)
和 [Boost.Beast](https://www.boost.org/doc/libs/release/libs/beast/) 构建的现代 C++20 HTTP 客户端库。

`httpc` 提供了一个小巧、对协程友好的 `http_client`，支持 HTTP 和
HTTPS、重定向、分块流式上传、文件上传/下载、TLS SNI 以及可配置的超时。

## 特性

- 通过 `boost::beast` 支持 HTTP / HTTPS
- C++20 协程（`boost::asio::awaitable`）
- 自动跟随重定向（`301/302/303/307/308`）
- 通过传输回调或直接写入文件进行流式下载
- 从文件（`async_upload_file`）或生产者回调
  （`async_upload_stream`，分块）进行流式上传
- 可选的证书校验和自定义 CA 证书包
- 可配置的连接 / 读写超时以及 TLS SNI

## 依赖要求

- CMake 3.16+
- 支持 C++20 的编译器（GCC 10+、Clang 12+、MSVC 19.29+）
- Boost 1.81+（`url`、`system`；`asio`、`beast`、`variant2` 为仅头文件）
- OpenSSL

## 构建

使用已安装的 Boost：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

使用内置的 Boost 源码树，位于 `third_party/boost` 下的源码树会被自动识别，
也可以显式指定路径：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DHTTPC_BOOST_ROOT=/path/to/boost
cmake --build build -j
```

选项：

| 选项                         | 默认值  | 说明                                            |
| ---------------------------- | ------- | ----------------------------------------------- |
| `HTTPC_BUILD_EXAMPLES`       | `OFF`   | 构建示例程序。                                  |
| `HTTPC_SEPARATE_COMPILATION` | `ON`    | 单独编译 Boost.Asio/Beast（更快）。             |
| `HTTPC_BUILD_TESTS`          | `OFF`   | 构建单元测试与集成测试。                        |
| `HTTPC_BOOST_ROOT`           | (自动)  | Boost 源码树的路径。                            |

构建示例：

```sh
cmake -S . -B build -DHTTPC_BUILD_EXAMPLES=ON
cmake --build build -j
./build/examples/httpc_get https://example.com/
```

### 疑难排查

如果构建时从默认搜索路径（例如 `/usr/local/include`）中拾取了旧版
Boost 的头文件，而不是 `find_package` 所选中的版本，可以通过
`-DHTTPC_BOOST_ROOT=...` 指向匹配的 Boost 源码树。这是确保编译时使用的
Boost 版本符合预期的最可靠方式。

## 测试

测试基于 [Boost.Test](https://www.boost.org/doc/libs/release/libs/test/)
编写，并通过 CTest 注册。测试包含两部分：

- 单元测试：URL 到请求目标 / Host 头的转换、请求头拷贝等纯逻辑。
- 集成测试：在本地回环地址上启动一个极简 HTTP 服务器，覆盖请求发送、
  重定向跟随、下载到文件与传输回调、文件上传、分块流式上传等场景。
  测试只使用 `127.0.0.1`，不依赖外网。

```sh
cmake -S . -B build -DHTTPC_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

也可以直接运行测试可执行文件，并按需选择用例：

```sh
./build/tests/httpc_tests
./build/tests/httpc_tests --run_test=client_suite/upload_stream --log_level=test_suite
```

使用 Boost 源码树时，测试会直接把 `libs/test/src` 下的 Boost.Test 实现编译
进测试目标，无需预先构建 Boost.Test 库；使用已安装的 Boost 时则通过
`find_package(Boost COMPONENTS unit_test_framework)` 链接。

## 持续集成

`.github/workflows/ci.yml` 覆盖多平台与多编译器组合：

| 平台    | 编译器                        |
| ------- | ----------------------------- |
| Linux   | GCC 13 / GCC 14 / Clang 18（含 libc++） |
| macOS   | AppleClang / GCC 14           |
| Windows | MSVC / clang-cl / MinGW（MSYS2 UCRT64） |

每个组合都会以 `Release` 配置构建库、示例与测试，并执行 `ctest`。

## 用法

```cpp
#include "httpc/httpc.hpp"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>

namespace net = boost::asio;

net::awaitable<void> fetch(std::string url)
{
    httpc::http_client client(co_await net::this_coro::executor);

    httpc::http_request req;
    req.method(httpc::verb::get);

    auto result = co_await client.async_perform(url, req);
    if (result)
        std::cout << "status: " << result->result_int() << '\n';
}

int main()
{
    net::io_context ioc;
    net::co_spawn(ioc, fetch("https://example.com/"), net::detached);
    ioc.run();
}
```

### 下载到文件

```cpp
httpc::http_client client(ex);
client.set_download_file("/tmp/file.bin");
client.max_redirects(10);

httpc::http_request req;
req.method(httpc::verb::get);

auto result = co_await client.async_perform(url, req);
```

### 上传文件

```cpp
httpc::http_request req;
req.method(httpc::verb::put);

auto result = co_await client.async_upload_file(url, "/path/to/file.iso", req);
```

### 从 CMake 链接

```cmake
find_package(httpc REQUIRED)
target_link_libraries(myapp PRIVATE httpc::httpc)
```

或者作为子目录使用时：

```cmake
add_subdirectory(httpc)
target_link_libraries(myapp PRIVATE httpc::httpc)
```

## 许可证

基于 Boost 软件许可证 1.0 版分发。
详见 [LICENSE](LICENSE)。

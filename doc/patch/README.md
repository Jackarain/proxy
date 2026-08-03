# third_party/boost 本地补丁

本目录保存对本仓库 `third_party/boost` 源码的本地修改(即上游未合入的补丁),
以便在整体升级 `third_party/boost` 时, 能够将这些修改重新应用到新的 boost 源码上.

## 补丁列表

| 文件 | 对应 commit | 说明 |
|---|---|---|
| `asio-ssl-batched-write.patch` | `213456e2d` (Batch TLS record writes in asio ssl engine) | 优化 asio `boost::asio::ssl` 的 TLS 写路径: 通过去除 `SSL_MODE_ENABLE_PARTIAL_WRITE`、增大 BIO pair 容量与读写缓冲, 使 `SSL_write` 一次加密并批量发送多个 TLS record, 大幅减少底层写 syscall 次数与每 record 的 asio 异步调度开销. HTTPS 代理 10G 传输实测 CONNECT 模式 555 → 817 MB/s. |

## 涉及文件

- `third_party/boost/libs/asio/include/boost/asio/ssl/detail/impl/engine.ipp`
- `third_party/boost/libs/asio/include/boost/asio/ssl/detail/stream_core.hpp`

## 应用方法

在仓库根目录执行(两种方式任选其一):

```sh
# 方式一: git apply(推荐, 可带 --check 先验证)
git apply --check doc/patch/asio-ssl-batched-write.patch
git apply doc/patch/asio-ssl-batched-write.patch

# 方式二: patch -p1
patch -p1 < doc/patch/asio-ssl-batched-write.patch
```

## 升级 third_party/boost 后的应用流程

1. 用新版 boost 整体替换 `third_party/boost`(此时补丁对应的旧修改会丢失).
2. 在仓库根目录执行上面的 `git apply` 命令.
3. 若新版 boost 的 asio 代码有变化导致补丁无法干净应用, `git apply` 会报错并提示冲突位置.
   此时需要手工对照补丁内容, 将修改移植到新代码上, 并更新补丁文件.
4. 重新编译并验证:
   ```sh
   cmake --build build-clang-sysopenssl --target proxy_server -j$(nproc)
   ```

## 重新生成补丁

修改了 asio 源码后, 可用以下命令重新生成补丁文件(以 `1f9e556b6` 为基准,
即 asio 被修改前的最后一次干净提交):

```sh
git diff 1f9e556b6 -- \
  third_party/boost/libs/asio/include/boost/asio/ssl/detail/impl/engine.ipp \
  third_party/boost/libs/asio/include/boost/asio/ssl/detail/stream_core.hpp \
  > doc/patch/asio-ssl-batched-write.patch
```

> 注意: 重新生成前请确认 `1f9e556b6` 之后对上述两个 asio 文件没有其他无关改动.

## 补丁内容摘要

`boost/asio/ssl/detail/impl/engine.ipp`:

- 删除两个构造函数中的 `SSL_set_mode(ssl_, SSL_MODE_ENABLE_PARTIAL_WRITE)`.
  `SSL_MODE_ENABLE_PARTIAL_WRITE` 会让 `SSL_write` 每次调用只处理一个 TLS record,
  导致每个 record 都触发一次完整的 asio 异步写状态机(一次底层写 syscall).
- 将 `BIO_new_bio_pair` 的写缓冲容量从 `0`(默认) 增大为 `256 * 1024`,
  使 `SSL_write` 一次能够在 BIO 中积攒多个 record.

`boost/asio/ssl/detail/stream_core.hpp`:

- 将 `max_tls_record_size` 枚举值从 `17 * 1024` 增大为 `256 * 1024`.
  该值同时决定 `output_buffer_space_` / `input_buffer_space_` 大小,
  使 `engine::get_output` 一次能读出批量密文, 一次底层写发送多个 record.

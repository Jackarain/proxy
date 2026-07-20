//
// httpc.hpp
// ~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/executor.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <boost/asio/experimental/awaitable_operators.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>

#include <boost/variant2.hpp>
#include <boost/system/result.hpp>

#include <boost/url/url_view.hpp>
#include <boost/url/parse.hpp>

#include <functional>
#include <string>
#include <chrono>
#include <cstdio>

namespace httpc {

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace ssl = net::ssl;
namespace urls = boost::urls;

using tcp = net::ip::tcp;

using http_request = http::request<http::string_body>;
using http_response = http::response<http::dynamic_body>;

template<class T>
using result = boost::system::result<T>;

using http_result = result<http_response>;
using verb = http::verb;

class http_client
{
public:
    using ssl_stream = beast::ssl_stream<beast::tcp_stream>;
    using ssl_stream_ptr = std::unique_ptr<ssl_stream>;

    using tcp_stream = beast::tcp_stream;
    using tcp_stream_ptr = std::unique_ptr<tcp_stream>;

    using variant_socket = boost::variant2::variant<tcp_stream_ptr, ssl_stream_ptr>;

    using executor_type = net::any_io_executor;

    using transfer_handler = std::function<int(void*, std::size_t)>;

public:
    // 构造函数.
    explicit http_client(net::any_io_executor ex, const net::const_buffer& ca_certs = {});
    ~http_client();

public:
    // ------------------------------------------------------------

    // 异步执行 HTTP 请求, 返回响应结果.
    // 使用示例:
    //
    //  http_client client(co_await net::this_coro::executor);
    //  client.set_transfer_handler([](auto data, auto size)
    //  {
    //    std::cout.write((const char*)data, size);
    //    return 0;
    //  });
    //  http_request req;
    //  req.method(httpc::verb::get);
    //  auto result = co_await client.async_perform(
    //      "https://example.com/file", req);
    //  if (result)
    //      auto& resp = *result;    // http_response
    //
    net::awaitable<http_result>
    async_perform(const std::string& url, const http_request& req) noexcept;

    // 异步上传文件到服务器.
    // 使用 http::file_body 流式上传, 支持重定向.
    //
    // 使用示例:
    //
    //  http_client client(co_await net::this_coro::executor);
    //  http_request req;
    //  req.method(httpc::verb::put);
    //  req.set(httpc::http::field::authorization, "Bearer xxx");
    //  auto result = co_await client.async_upload_file(
    //      "https://example.com/upload",
    //      "/path/to/large_file.iso",
    //      req);
    //  if (result)
    //      auto& resp = *result;    // http_response
    //
    net::awaitable<http_result> async_upload_file(
        const std::string& url,
        const std::string& file_path,
        const http_request& req = http_request {}) noexcept;

    // 异步上传流数据.
    // 使用 upload_handler 作为数据源流式上传, 支持重定向.
    //
    // 使用示例:
    //
    //  http_client client(co_await net::this_coro::executor);
    //  client.set_transfer_handler([](void* data, std::size_t size) -> int {
    //      return ::read(fd, data, size);
    //  });
    //  co_await client.async_upload_stream(
    //      "https://example.com/upload", req);
    //
    net::awaitable<http_result>
    async_upload_stream(const std::string& url, const http_request& req) noexcept;

    // ------------------------------------------------------------
    // 以下接口为手工精细控制.

    // 连接 URL 指向的服务器.
    net::awaitable<boost::system::error_code> async_connect(const urls::url_view& url);

    // 发送请求并接收响应.
    net::awaitable<boost::system::error_code>
    async_send_request(const urls::url_view& url, const http_request& req);

    // 读取完整响应 (处理下载文件/传输回调).
    net::awaitable<http_result> async_read_response();

    // 仅发送 HTTP 请求头.
    net::awaitable<boost::system::error_code>
    async_write_header(const urls::url_view& url, const http_request& req);

    // 写入 HTTP chunk (有数据时用 chunk_body, 空 buffer 时写入终止 chunk_last).
    net::awaitable<boost::system::error_code> async_write_chunk(net::const_buffer buffer);

    // 设置当前流的超时.
    void set_stream_timeout();

    // 清除当前流的超时.
    void clear_stream_timeout();

    // 关闭当前连接.
    void close();

    // ------------------------------------------------------------
    // 以下接口用于配置或获取 http_client 相关配置.

    // 设置下载到指定文件.
    void set_download_file(const std::string& file_path) noexcept;

    // 设置传输回调函数, 用于获取传输进度.
    void set_transfer_handler(transfer_handler&& handler) noexcept;

    // 设置 User-agent.
    void user_agent(const std::string& ua) noexcept;

    // 检查和设置证书认证是否启用 (默认关闭).
    bool check_certificate() const noexcept;
    void check_certificate(bool check) noexcept;

    // 设置是否跟随重定向 (默认开启).
    bool follow_redirect() const noexcept;
    void follow_redirect(bool follow) noexcept;

    // 设置最大重定向次数 (默认 5).
    int max_redirects() const noexcept;
    void max_redirects(int n) noexcept;

    // 设置连接超时时间 (毫秒, 默认 30 秒).
    std::chrono::milliseconds connect_timeout() const noexcept;
    void connect_timeout(std::chrono::milliseconds ms) noexcept;

    // 设置读写超时时间 (毫秒, 默认 30 秒).
    std::chrono::milliseconds timeout() const noexcept;
    void timeout(std::chrono::milliseconds ms) noexcept;

    // 获取执行器.
    net::any_io_executor get_executor() noexcept;

private:
    // 执行器.
    net::any_io_executor executor_;

    // SSL 上下文 (必须在 stream_socket_ 之前初始化).
    ssl::context ssl_ctx_ {ssl::context::tls_client};

    // 变体 socket (TCP 或 SSL).
    variant_socket stream_socket_;

    // 缓冲区.
    beast::flat_buffer buffer_;

    // 下载文件路径.
    std::string download_file_path_;

    // 传输回调.
    transfer_handler transfer_handler_;

    // 自定义 fclose 删除器.
    struct fclose_deleter
    {
        void operator()(FILE* f) const noexcept
        {
            std::fclose(f);
        }
    };

    // 下载文件句柄 (RAII).
    std::unique_ptr<FILE, fclose_deleter> download_file_ {nullptr};

    // User-agent.
    std::string user_agent_;

    // 证书验证开关.
    bool check_certificate_ {false};

    // 重定向设置.
    bool follow_redirect_ {true};
    int max_redirects_ {5};

    // 超时设置, 默认永不超时.
    std::chrono::milliseconds connect_timeout_ {std::chrono::milliseconds::max()};
    std::chrono::milliseconds timeout_ {std::chrono::milliseconds::max()};
};

} // namespace httpc

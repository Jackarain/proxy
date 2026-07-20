//
// httpc.cpp
// ~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "httpc/httpc.hpp"

#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>

#include <boost/beast/http/read.hpp>
#include <boost/beast/http/write.hpp>

#include <boost/core/ignore_unused.hpp>

#if defined(BUILD_ASIO_SSL_AND_BEAST_SRC)
# include <boost/asio/ssl/impl/src.hpp>
# include <boost/beast/src.hpp>
#endif

#include <charconv>
#include <fstream>
#include <sstream>
#include <system_error>

namespace httpc {

// 默认 User-Agent.
inline const std::string default_user_agent = "httpc/1.0 (Boost.Beast)";

// 常用 User-Agent 常量.
inline const std::string chrome_user_agent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                                             "AppleWebKit/537.36 (KHTML, like Gecko) "
                                             "Chrome/120.0.0.0 Safari/537.36";

inline const std::string firefox_user_agent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:120.0) "
                                              "Gecko/20100101 Firefox/120.0";

// -----------------------------------------------------------------------
// 辅助函数 (匿名命名空间)
// -----------------------------------------------------------------------

namespace {

    // 从 URL 构建 Host 头值.
    std::string build_host_header(const urls::url_view& url)
    {
        std::string host_value(url.host());
        auto port = url.port_number();
        if (port != 0)
        {
            if ((url.scheme_id() == urls::scheme::https && port != 443)
                || (url.scheme_id() != urls::scheme::http && port != 80))
            {
                host_value += ':';
                host_value += std::to_string(port);
            }
        }
        return host_value;
    }

    // 从 URL 构建请求目标 (path + query).
    std::string build_request_target(const urls::url_view& url)
    {
        std::string target(url.encoded_path());
        if (url.has_query())
        {
            target += '?';
            target.append(url.encoded_query().data(), url.encoded_query().size());
        }
        if (target.empty())
            target = "/";
        return target;
    }

    // 在任意 body 类型的请求上设置来自 URL 的目标和 Host.
    template<typename Body>
    void setup_request_from_url(http::request<Body>& req, const urls::url_view& url)
    {
        auto target = build_request_target(url);
        req.target(target);
        if (req.find(http::field::host) == req.end())
        {
            auto host = build_host_header(url);
            req.set(http::field::host, host);
        }
    }

    // 将源请求的通用头部复制到目标请求.
    template<typename Body>
    void copy_request_headers(http::request<Body>& req, const http_request& source)
    {
        // Host
        {
            auto it = source.find(http::field::host);
            if (it != source.end())
                req.set(http::field::host, it->value());
        }

        // User-Agent
        {
            auto it = source.find(http::field::user_agent);
            if (it != source.end())
                req.set(http::field::user_agent, it->value());
            else
                req.set(http::field::user_agent, default_user_agent);
        }

        // Connection
        {
            auto it = source.find(http::field::connection);
            if (it != source.end())
                req.set(http::field::connection, it->value());
            else
                req.keep_alive(false);
        }

        // 拷贝用户自定义请求头 (排除已处理字段).
        for (auto const& h : source)
        {
            if (h.name() != http::field::host && h.name() != http::field::user_agent
                && h.name() != http::field::connection)
            {
                req.set(h.name_string(), h.value());
            }
        }
    }

    // 重定向循环: 连接→发送→读取→重定向检查.
    template<typename SendFunc>
    net::awaitable<http_result>
    async_perform_loop(http_client& client, const std::string& url, SendFunc&& send)
    {
        boost::system::error_code ec;
        std::string current_url = url;
        int redirect_count = 0;

        while (true)
        {
            auto url_result = urls::parse_uri_reference(current_url);
            if (!url_result)
                co_return url_result.error();
            auto url_view = *url_result;

            ec = co_await client.async_connect(url_view);
            if (ec)
                co_return ec;

            client.clear_stream_timeout();

            ec = co_await send(url_view);
            if (ec)
                co_return ec;

            auto result = co_await client.async_read_response();
            if (!result)
                co_return result.error();

            auto& resp = *result;

            if (redirect_count < client.max_redirects())
            {
                auto status = resp.result_int();
                if (status == 301 || status == 302 || status == 303 || status == 307
                    || status == 308)
                {
                    auto location = resp.find(http::field::location);
                    if (location != resp.end())
                    {
                        std::string new_url(location->value());
                        auto new_url_result = urls::parse_uri_reference(new_url);
                        if (new_url_result)
                        {
                            current_url = new_url;
                            redirect_count++;
                            client.close();
                            continue;
                        }
                    }
                }
            }

            co_return result;
        }
    }

} // anonymous namespace

// -----------------------------------------------------------------------
// 构造 / 析构
// -----------------------------------------------------------------------

http_client::http_client(net::any_io_executor ex, const net::const_buffer& ca_certs)
    : executor_(ex)
    , user_agent_(default_user_agent)
{
    // 分配保留缓存空间.
    buffer_.reserve(5 * 1024 * 1024);

    // 配置 SSL 上下文
    ssl_ctx_.set_default_verify_paths();
    ssl_ctx_.set_verify_mode(ssl::verify_peer);

    // 若用户不检查证书, 则禁用证书验证.
    if (!check_certificate_)
    {
        ssl_ctx_.set_verify_callback(
            [](bool, ssl::verify_context&)
            {
                return true;
            });
    }
    else
    {
        if (ca_certs.size() > 0)
		    ssl_ctx_.add_certificate_authority(
                net::buffer(ca_certs.data(), ca_certs.size()));
    }
}

http_client::~http_client()
{
    close();
}

// -----------------------------------------------------------------------
// 连接管理
// -----------------------------------------------------------------------

net::awaitable<boost::system::error_code> http_client::async_connect(const urls::url_view& url)
{
    using namespace boost::asio::experimental::awaitable_operators;
    boost::system::error_code ec;

    // 判断协议
    bool use_ssl = (url.scheme_id() == urls::scheme::https);

    // 端口号
    std::uint16_t port = url.port_number();
    if (port == 0)
        port = use_ssl ? 443 : 80;

    // 主机名
    auto host = url.host();
    if (host.empty())
    {
        ec = boost::system::errc::make_error_code(boost::system::errc::invalid_argument);
        co_return ec;
    }

    // 关闭旧连接
    close();

    // 解析 DNS
    tcp::resolver resolver(co_await net::this_coro::executor);
    auto resolve_result =
        co_await resolver.async_resolve(host, std::to_string(port), net::redirect_error(ec));
    if (ec)
        co_return ec;

    struct clear_timeout
    {
        variant_socket& stream_socket_;

        ~clear_timeout()
        {
            auto visitor = [](auto& stream_ptr)
            {
                if (!stream_ptr)
                    return;
                using stream_t = std::decay_t<decltype(*stream_ptr)>;
                if constexpr (std::is_same_v<stream_t, ssl_stream>)
                    stream_ptr->next_layer().expires_never();
                else
                    stream_ptr->expires_never();
            };
            boost::variant2::visit(visitor, stream_socket_);
        }
    } timeout_defer {this->stream_socket_};

    // 1. 创建并连接 TCP 流
    auto tcp_sock = std::make_unique<tcp_stream>(co_await net::this_coro::executor);
    tcp_sock->expires_never();
    bool has_timeout = connect_timeout_ == std::chrono::milliseconds::max();

    if (use_ssl)
    {
        // ---- HTTPS ----
        if (!has_timeout)
            tcp_sock->expires_after(connect_timeout_);

        co_await tcp_sock->async_connect(resolve_result, net::redirect_error(ec));
        if (ec)
            co_return ec;

        // 2. 构造 SSL 流 (接管已连接的 TCP 流)
        auto ssl_sock = std::make_unique<ssl_stream>(std::move(*tcp_sock), ssl_ctx_);
        // tcp_sock 此时为空 (moved-from)

        // 3. SSL 握手
        if (!has_timeout)
            ssl_sock->next_layer().expires_after(connect_timeout_);
        co_await ssl_sock->async_handshake(ssl::stream_base::client, net::redirect_error(ec));
        if (ec)
            co_return ec;

        // 存入 variant
        stream_socket_.emplace<ssl_stream_ptr>(std::move(ssl_sock));
    }
    else
    {
        // ---- HTTP ----
        if (!has_timeout)
            tcp_sock->expires_after(connect_timeout_);
        co_await tcp_sock->async_connect(resolve_result, net::redirect_error(ec));
        if (ec)
            co_return ec;

        stream_socket_.emplace<tcp_stream_ptr>(std::move(tcp_sock));
    }

    co_return ec;
}

// -----------------------------------------------------------------------
// 发送请求 & 接收响应
// -----------------------------------------------------------------------

net::awaitable<boost::system::error_code>
http_client::async_send_request(const urls::url_view& url, const http_request& req)
{
    boost::system::error_code ec;

    // 构造请求副本, 以便修改目标 / Host
    http_request request = req;

    // 使用辅助函数设置目标路径和 Host 头.
    setup_request_from_url(request, url);

    // 设置 User-Agent (如果未设置)
    if (!user_agent_.empty())
        request.set(http::field::user_agent, user_agent_);

    // 设置 Connection 头
    if (request.find(http::field::connection) == request.end())
        request.keep_alive(false);

    // 发送请求
    set_stream_timeout();
    auto send_visitor = [&](auto& stream_ptr) -> net::awaitable<boost::system::error_code>
    {
        boost::system::error_code ec;
        co_await http::async_write(*stream_ptr, request, net::redirect_error(ec));
        co_return ec;
    };

    ec = co_await boost::variant2::visit(send_visitor, stream_socket_);
    clear_stream_timeout();

    co_return ec;
}

// -----------------------------------------------------------------------
// 读取响应 (支持下载文件 / 传输回调)
// -----------------------------------------------------------------------

net::awaitable<http_result> http_client::async_read_response()
{
    http::response_parser<http::dynamic_body> parser;
    parser.eager(true);
    parser.body_limit(std::numeric_limits<std::uint64_t>::max());

    boost::system::error_code ec;

    // 读取 header
    set_stream_timeout();
    auto read_header_visitor = [&](auto& stream_ptr) -> net::awaitable<boost::system::error_code>
    {
        boost::system::error_code ec;
        co_await http::async_read_header(*stream_ptr, buffer_, parser, net::redirect_error(ec));
        co_return ec;
    };

    ec = co_await boost::variant2::visit(read_header_visitor, stream_socket_);
    clear_stream_timeout();
    if (ec)
        co_return ec;

    // 如果有 body, 则分块读取
    if (!parser.is_done())
    {
        // 如果需要下载到文件, 打开文件
        if (!download_file_path_.empty())
        {
            FILE* file = std::fopen(download_file_path_.c_str(), "wb");
            if (!file)
            {
                ec = boost::system::errc::make_error_code(
                    boost::system::errc::no_such_file_or_directory);
                co_return ec;
            }
            download_file_.reset(file);
        }

        // 分块读取 body
        while (!parser.is_done())
        {
            set_stream_timeout();

            // 读一块
            auto read_some_visitor =
                [&](auto& stream_ptr) -> net::awaitable<boost::system::error_code>
            {
                boost::system::error_code ec;
                co_await http::async_read_some(
                    *stream_ptr,
                    buffer_,
                    parser,
                    net::redirect_error(ec));
                co_return ec;
            };

            ec = co_await boost::variant2::visit(read_some_visitor, stream_socket_);
            clear_stream_timeout();

            // 处理 body 数据 (从 parser 中的 message 获取)
            auto& msg = parser.get();
            auto& body = msg.body();
            for (auto const& buf : body.data())
            {
                auto data = static_cast<const char*>(buf.data());
                auto size = buf.size();
                if (size == 0)
                    continue;

                // 写入文件
                if (download_file_)
                {
                    std::fwrite(data, 1, size, download_file_.get());
                }

                // 传输回调
                if (transfer_handler_)
                {
                    transfer_handler_((char*)data, size);
                }
            }
            body.clear();

            // 检查错误
            if (ec)
            {
                if (ec == http::error::partial_message || ec == http::error::need_buffer)
                {
                    // 部分数据已处理, 继续读取下一块
                    ec = {};
                    continue;
                }
                if (ec == http::error::end_of_stream)
                {
                    ec = {};
                    break;
                }
                co_return ec;
            }
        }

        // 关闭下载文件
        download_file_.reset();
    }

    // 获取完整响应对象
    http_response resp = parser.release();

    // 重置 buffer
    buffer_.clear();

    co_return std::move(resp);
}

// -----------------------------------------------------------------------
// async_perform 主入口
// -----------------------------------------------------------------------

net::awaitable<http_result>
http_client::async_perform(const std::string& url, const http_request& req) noexcept
{
    auto send_func =
        [&](const urls::url_view& url_view) -> net::awaitable<boost::system::error_code>
    {
        co_return co_await async_send_request(url_view, req);
    };

    co_return co_await async_perform_loop(*this, url, std::move(send_func));
}

// -----------------------------------------------------------------------
// async_upload_file
// -----------------------------------------------------------------------

net::awaitable<http_result> http_client::async_upload_file(
    const std::string& url, const std::string& file_path, const http_request& req) noexcept
{
    auto send_func =
        [&](const urls::url_view& url_view) -> net::awaitable<boost::system::error_code>
    {
        boost::system::error_code ec;

        // 构造 file_body 请求
        http::request<http::file_body> file_req {req.method(), req.target(), req.version()};

        // 设置目标路径, Host, User-Agent, Connection 等.
        setup_request_from_url(file_req, url_view);
        copy_request_headers(file_req, req);

        // 打开文件
        http::file_body::value_type file;
        file.open(file_path.c_str(), beast::file_mode::scan, ec);
        if (ec)
            co_return ec;
        file_req.body() = std::move(file);
        file_req.prepare_payload(); // 设置 Content-Length

        // 发送请求 (流式读取文件并写入 socket)
        set_stream_timeout();
        auto send_visitor = [&](auto& stream_ptr) -> net::awaitable<boost::system::error_code>
        {
            boost::system::error_code ec;
            co_await http::async_write(*stream_ptr, file_req, net::redirect_error(ec));
            co_return ec;
        };
        ec = co_await boost::variant2::visit(send_visitor, stream_socket_);
        clear_stream_timeout();

        co_return ec;
    };

    co_return co_await async_perform_loop(*this, url, std::move(send_func));
}

// -----------------------------------------------------------------------
// async_upload_stream
// -----------------------------------------------------------------------

net::awaitable<http_result>
http_client::async_upload_stream(const std::string& url, const http_request& req) noexcept
{
    auto send_func =
        [&](const urls::url_view& url_view) -> net::awaitable<boost::system::error_code>
    {
        boost::system::error_code ec;

        set_stream_timeout();

        // 使用 async_write_header 发送 chunked 请求头
        ec = co_await async_write_header(url_view, req);
        if (ec)
            co_return ec;

        clear_stream_timeout();

        // 流式上传 body (chunked)
        if (transfer_handler_)
        {
            char buf[65536];

            while (true)
            {
                int n = transfer_handler_(buf, sizeof(buf));
                if (n <= 0)
                    break;

                set_stream_timeout();

                // 写入 chunk
                ec = co_await async_write_chunk(net::buffer(buf, static_cast<std::size_t>(n)));
                if (ec)
                    co_return ec;

                clear_stream_timeout();
            }

            set_stream_timeout();

            // 写入 final chunk (0\r\n\r\n)
            ec = co_await async_write_chunk(net::const_buffer {});
            if (ec)
                co_return ec;

            clear_stream_timeout();
        }
        else
        {
            set_stream_timeout();

            // 没有设置 transfer_handler, 发送空 chunked body
            ec = co_await async_write_chunk(net::const_buffer {});
            if (ec)
                co_return ec;

            set_stream_timeout();
        }

        co_return ec;
    };

    co_return co_await async_perform_loop(*this, url, std::move(send_func));
}

// -----------------------------------------------------------------------
// async_write_header
// -----------------------------------------------------------------------

net::awaitable<boost::system::error_code>
http_client::async_write_header(const urls::url_view& url, const http_request& req)
{
    boost::system::error_code ec;

    // 构造请求 (使用 empty_body, 手动写入 chunked body)
    http::request<http::empty_body> stream_req {req.method(), req.target(), req.version()};

    // 使用辅助函数设置目标路径, Host, User-Agent, Connection 等.
    setup_request_from_url(stream_req, url);
    copy_request_headers(stream_req, req);

    // 设置 chunked 传输编码
    stream_req.chunked(true);

    // 创建 serializer 用于写请求头
    http::request_serializer<http::empty_body> sr {stream_req};

    // 写请求头
    set_stream_timeout();
    auto write_header_visitor = [&](auto& stream_ptr) -> net::awaitable<boost::system::error_code>
    {
        boost::system::error_code ec;
        co_await http::async_write_header(*stream_ptr, sr, net::redirect_error(ec));
        co_return ec;
    };

    ec = co_await boost::variant2::visit(write_header_visitor, stream_socket_);
    clear_stream_timeout();

    co_return ec;
}

// -----------------------------------------------------------------------
// 设置超时 / 关闭连接
// -----------------------------------------------------------------------

void http_client::set_stream_timeout()
{
    if (timeout_ == std::chrono::milliseconds::max())
        return;

    auto visitor = [this](auto& stream_ptr)
    {
        using stream_t = std::decay_t<decltype(*stream_ptr)>;
        if constexpr (std::is_same_v<stream_t, ssl_stream>)
            stream_ptr->next_layer().expires_after(timeout_);
        else
            stream_ptr->expires_after(timeout_);
    };
    boost::variant2::visit(visitor, stream_socket_);
}

void http_client::clear_stream_timeout()
{
    if (timeout_ == std::chrono::milliseconds::max())
        return;

    auto visitor = [](auto& stream_ptr)
    {
        if (!stream_ptr)
            return;
        using stream_t = std::decay_t<decltype(*stream_ptr)>;
        if constexpr (std::is_same_v<stream_t, ssl_stream>)
            stream_ptr->next_layer().expires_never();
        else
            stream_ptr->expires_never();
    };
    boost::variant2::visit(visitor, stream_socket_);
}

net::awaitable<boost::system::error_code> http_client::async_write_chunk(net::const_buffer buffer)
{
    auto write_visitor = [&](auto& stream_ptr) -> net::awaitable<boost::system::error_code>
    {
        boost::system::error_code ec;
        if (buffer.size() > 0)
        {
            auto chunk = http::chunk_body(buffer);
            co_await net::async_write(*stream_ptr, chunk, net::redirect_error(ec));
        }
        else
        {
            http::chunk_last last_chunk;
            co_await net::async_write(*stream_ptr, last_chunk, net::redirect_error(ec));
        }
        co_return ec;
    };
    co_return co_await boost::variant2::visit(write_visitor, stream_socket_);
}

void http_client::close()
{
    auto visitor = [](auto& stream_ptr)
    {
        if (!stream_ptr)
            return;

        beast::error_code ec;
        using stream_t = std::decay_t<decltype(*stream_ptr)>;
        if constexpr (std::is_same_v<stream_t, ssl_stream>)
            stream_ptr->next_layer().socket().close(ec);
        else
            stream_ptr->socket().close(ec);
        boost::ignore_unused(ec);
    };
    boost::variant2::visit(visitor, stream_socket_);
    buffer_.clear();
}

// -----------------------------------------------------------------------
// Getter / Setter
// -----------------------------------------------------------------------

void http_client::set_download_file(const std::string& file_path) noexcept
{
    download_file_path_ = file_path;
}

void http_client::set_transfer_handler(transfer_handler&& handler) noexcept
{
    transfer_handler_ = std::move(handler);
}

void http_client::user_agent(const std::string& ua) noexcept
{
    user_agent_ = ua;
}

bool http_client::check_certificate() const noexcept
{
    return check_certificate_;
}

void http_client::check_certificate(bool check) noexcept
{
    check_certificate_ = check;
    if (check)
    {
        ssl_ctx_.set_verify_mode(ssl::verify_peer);
        ssl_ctx_.set_default_verify_paths();
    }
    else
    {
        ssl_ctx_.set_verify_mode(ssl::verify_none);
    }
}

int http_client::max_redirects() const noexcept
{
    return max_redirects_;
}

void http_client::max_redirects(int n) noexcept
{
    max_redirects_ = n;
}

std::chrono::milliseconds http_client::connect_timeout() const noexcept
{
    return connect_timeout_;
}

void http_client::connect_timeout(std::chrono::milliseconds ms) noexcept
{
    connect_timeout_ = ms;
}

std::chrono::milliseconds http_client::timeout() const noexcept
{
    return timeout_;
}

void http_client::timeout(std::chrono::milliseconds ms) noexcept
{
    timeout_ = ms;
}

net::any_io_executor http_client::get_executor() noexcept
{
    return executor_;
}

} // namespace httpc

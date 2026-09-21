//
// test_helpers.hpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include "httpc/httpc.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <boost/system/error_code.hpp>

#include <chrono>
#include <functional>
#include <future>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>

namespace httpc_test {

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = net::ip::tcp;

// 测试服务器对每个请求的响应回调.
using server_handler =
    std::function<http::response<http::string_body>(const http::request<http::string_body>&)>;

// 极简的本地 HTTP 服务器, 运行在独立线程上, 为测试提供确定性的响应.
class test_server
{
public:
    test_server()
        : work_guard_(net::make_work_guard(io_context_))
    {
        boost::system::error_code ec;
        tcp::endpoint endpoint(net::ip::make_address("127.0.0.1", ec), 0);
        if (ec)
            throw std::runtime_error("cannot parse loopback address: " + ec.message());

        acceptor_.open(endpoint.protocol(), ec);
        if (ec)
            throw std::runtime_error("cannot open acceptor: " + ec.message());

        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        acceptor_.bind(endpoint, ec);
        if (ec)
            throw std::runtime_error("cannot bind acceptor: " + ec.message());

        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec)
            throw std::runtime_error("cannot listen: " + ec.message());

        port_ = acceptor_.local_endpoint().port();
    }

    ~test_server()
    {
        boost::system::error_code ec;
        acceptor_.close(ec);
        work_guard_.reset();
        io_context_.stop();
        if (thread_.joinable())
            thread_.join();
    }

    test_server(const test_server&) = delete;
    test_server& operator=(const test_server&) = delete;

    // 启动服务器线程并安装响应回调.
    void start(server_handler handler)
    {
        handler_ = std::move(handler);
        thread_ = std::thread(
            [this]
            {
                net::co_spawn(io_context_, accept_loop(), net::detached);
                io_context_.run();
            });
    }

    std::uint16_t port() const noexcept
    {
        return port_;
    }

    // 构造指向本服务器的 URL.
    std::string url(const std::string& target = "/") const
    {
        return "http://127.0.0.1:" + std::to_string(port_) + target;
    }

private:
    net::awaitable<void> accept_loop()
    {
        boost::system::error_code ec;
        while (true)
        {
            auto socket =
                co_await acceptor_.async_accept(net::redirect_error(net::use_awaitable, ec));
            if (ec)
                co_return;
            net::co_spawn(io_context_, session(std::move(socket)), net::detached);
        }
    }

    net::awaitable<void> session(tcp::socket socket)
    {
        beast::tcp_stream stream(std::move(socket));
        beast::flat_buffer buffer;
        boost::system::error_code ec;

        while (true)
        {
            http::request<http::string_body> req;
            co_await http::async_read(stream, buffer, req, net::redirect_error(ec));
            if (ec)
                co_return;

            http::response<http::string_body> res = handler_(req);
            res.keep_alive(req.keep_alive());
            res.prepare_payload();

            co_await http::async_write(stream, res, net::redirect_error(ec));
            if (ec)
                co_return;

            if (!req.keep_alive())
                break;
        }

        stream.socket().shutdown(tcp::socket::shutdown_send, ec);
    }

    net::io_context io_context_;
    net::executor_work_guard<net::io_context::executor_type> work_guard_;
    tcp::acceptor acceptor_ {io_context_};
    std::thread thread_;
    server_handler handler_;
    std::uint16_t port_ {0};
};

// 取出 awaitable 的值类型.
template<typename T>
struct awaitable_value
{
    using type = T;
};

template<typename T, typename Executor>
struct awaitable_value<net::awaitable<T, Executor>>
{
    using type = T;
};

template<typename T>
using awaitable_value_t = typename awaitable_value<std::remove_cvref_t<T>>::type;

// 在独立 io_context 上同步执行协程; 超时则停止 io_context 并抛出异常.
// 客户端在协程内部创建, 因此其执行器与驱动协程的 io_context 必然一致.
namespace detail {

// 在独立 io_context 上驱动协程并等待其结果, 带整体超时保护.
template<typename T>
T drive(net::awaitable<T> work, std::chrono::milliseconds timeout)
{
    net::io_context ioc;
    auto guard = net::make_work_guard(ioc);

    auto started = net::co_spawn(ioc, std::move(work), net::use_future);
    std::thread runner([&ioc] { ioc.run(); });

    if (started.wait_for(timeout) != std::future_status::ready)
    {
        ioc.stop();
        runner.join();
        throw std::runtime_error("test coroutine timed out");
    }

    T value = started.get();

    guard.reset();
    ioc.stop();
    runner.join();

    return value;
}

} // namespace detail

// 在独立 io_context 上执行协程; 客户端在协程内部创建, 因此其执行器与驱动
// 协程的 io_context 必然一致.
template<typename Coroutine>
auto run_sync(Coroutine coroutine, std::chrono::milliseconds timeout = std::chrono::seconds(30))
{
    using value_t = awaitable_value_t<std::invoke_result_t<Coroutine&, httpc::http_client&>>;

    return detail::drive(
        [](Coroutine body) -> net::awaitable<value_t>
        {
            httpc::http_client client(co_await net::this_coro::executor);
            co_return co_await body(client);
        }(std::move(coroutine)),
        timeout);
}

// 先对客户端做配置 (同步 lambda 或协程), 再执行协程并返回其结果.
template<typename Configure, typename Coroutine>
auto run_sync_with_client(Configure configure, Coroutine coroutine,
    std::chrono::milliseconds timeout = std::chrono::seconds(30))
{
    using value_t = awaitable_value_t<std::invoke_result_t<Coroutine&, httpc::http_client&>>;

    return detail::drive(
        [](Configure setup, Coroutine body) -> net::awaitable<value_t>
        {
            httpc::http_client client(co_await net::this_coro::executor);

            if constexpr (std::is_void_v<std::invoke_result_t<Configure&, httpc::http_client&>>)
                setup(client);
            else
                co_await setup(client);

            co_return co_await body(client);
        }(std::move(configure), std::move(coroutine)),
        timeout);
}

} // namespace httpc_test

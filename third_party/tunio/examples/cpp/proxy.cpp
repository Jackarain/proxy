//
// proxy.cpp
// ~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// 代理 URL 解析与工厂实现。
//
// 说明：Boost.URL 自 1.92 起废弃 <boost/url/src.hpp> 的 header-only 集成
// 方式，要求链接独立编译的 boost_url 库；本示例为避免新增链接依赖，内置
// 仅解析 [scheme://][user:pass@]host[:port] 所需的最小逻辑，支持 IPv6
// 字面量与 userinfo 的 percent-decode。

#include "proxy.hpp"

#include "direct_proxy.hpp"
#include "http_proxy.hpp"
#include "socks5_client.hpp"

#include <boost/asio/experimental/awaitable_operators.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>

namespace tun2socks_example {

namespace {

char ascii_lower(char c) noexcept
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

std::string to_lower(std::string_view text)
{
    std::string out(text);
    for (char& c : out) {
        c = ascii_lower(c);
    }
    return out;
}

int hex_value(char c) noexcept
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

std::string percent_decode(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '%' && i + 2 < text.size()) {
            const int hi = hex_value(text[i + 1]);
            const int lo = hex_value(text[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        out.push_back(text[i]);
    }
    return out;
}

uint16_t default_port_for(std::string_view scheme) noexcept
{
    return scheme == "http" ? 8080 : 1080;
}

} // namespace

bool parse_proxy_url(std::string_view text, proxy_config& out, std::string& err)
{
    out = proxy_config{};
    err.clear();

    if (text.empty()) {
        err = "代理地址不能为空";
        return false;
    }

    std::string rest(text);

    // 无 scheme 时按 socks5 处理，兼容旧的 host:port 写法。
    std::string scheme = "socks5";
    const auto scheme_pos = rest.find("://");
    if (scheme_pos != std::string::npos) {
        scheme = to_lower(rest.substr(0, scheme_pos));
        rest = rest.substr(scheme_pos + 3);
    }

    if (scheme != "socks5" && scheme != "http" && scheme != "direct" &&
        scheme != "reject") {
        err = "不支持的代理协议: " + scheme;
        return false;
    }
    out.scheme = scheme;

    if (scheme == "direct" || scheme == "reject") {
        return true;
    }

    // 拆分 userinfo 与 host:port（userinfo 取最后一个 '@'）。
    std::string host_port = rest;
    const auto at = rest.rfind('@');
    if (at != std::string::npos) {
        const std::string userinfo = rest.substr(0, at);
        host_port = rest.substr(at + 1);
        const auto colon = userinfo.find(':');
        if (colon == std::string::npos) {
            out.user = percent_decode(userinfo);
        } else {
            out.user = percent_decode(userinfo.substr(0, colon));
            out.pass = percent_decode(userinfo.substr(colon + 1));
        }
    }

    std::string host;
    std::string port_text;
    if (!host_port.empty() && host_port.front() == '[') {
        const auto close = host_port.find(']');
        if (close == std::string::npos) {
            err = "非法 IPv6 代理主机: " + host_port;
            return false;
        }
        host = host_port.substr(1, close - 1);
        const std::string tail = host_port.substr(close + 1);
        if (!tail.empty()) {
            if (tail.front() != ':') {
                err = "非法代理地址: " + std::string(text);
                return false;
            }
            port_text = tail.substr(1);
        }
    } else {
        const auto colon = host_port.rfind(':');
        if (colon != std::string::npos) {
            host = host_port.substr(0, colon);
            port_text = host_port.substr(colon + 1);
        } else {
            host = host_port;
        }
    }

    if (host.empty()) {
        err = "代理 URL 缺少主机名: " + std::string(text);
        return false;
    }
    out.host = percent_decode(host);

    if (!port_text.empty()) {
        unsigned long value = 0;
        try {
            value = std::stoul(port_text);
        } catch (const std::exception&) {
            err = "非法代理端口: " + port_text;
            return false;
        }
        if (value == 0 || value > 65535) {
            err = "代理端口越界: " + port_text;
            return false;
        }
        out.port = static_cast<uint16_t>(value);
    } else {
        out.port = default_port_for(scheme);
    }
    return true;
}

net::awaitable<net::ip::tcp::socket> connect_tcp_with_timeout(
    net::any_io_executor ex, net::ip::tcp::endpoint target,
    std::chrono::milliseconds timeout)
{
    net::ip::tcp::socket sock(ex);
    net::steady_timer timer(ex);
    timer.expires_after(timeout);

    // Boost 1.81 无 cancel_after，用 awaitable_operators 的 || 做超时竞速：
    // 定时器先到则连接被取消，连接先完成则定时器被取消（两者均由库等待
    // 收尾，避免悬挂操作引用已销毁对象）。
    using namespace net::experimental::awaitable_operators;
    auto result = co_await (
        sock.async_connect(target, net::as_tuple(net::use_awaitable)) ||
        timer.async_wait(net::as_tuple(net::use_awaitable)));

    const std::string what = "connect " + target.address().to_string() + ":" +
        std::to_string(target.port());
    if (result.index() == 1) {
        throw boost::system::system_error(net::error::timed_out, what);
    }
    const auto &connect_result = std::get<0>(result);
    const boost::system::error_code ec = std::get<0>(connect_result);
    if (ec) {
        throw boost::system::system_error(ec, what);
    }
    co_return std::move(sock);
}

std::shared_ptr<proxy> make_proxy(const proxy_config& cfg,
    net::any_io_executor ex, std::chrono::milliseconds connect_timeout)
{
    if (cfg.scheme == "socks5") {
        return std::make_shared<socks5_proxy>(cfg, ex, connect_timeout);
    }
    if (cfg.scheme == "http") {
        return std::make_shared<http_proxy>(cfg, ex, connect_timeout);
    }
    if (cfg.scheme == "direct") {
        return std::make_shared<direct_proxy>(ex, connect_timeout);
    }
    if (cfg.scheme == "reject") {
        return std::make_shared<reject_proxy>();
    }
    return nullptr;
}

} // namespace tun2socks_example

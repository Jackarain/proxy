//
// detail/http_helpers.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>

#include <boost/url/url_view.hpp>

#include <string>

namespace httpc {

namespace beast = boost::beast;
namespace http = beast::http;
namespace urls = boost::urls;

namespace detail {

// 默认 User-Agent.
inline const std::string default_user_agent = "httpc/1.0 (Boost.Beast)";

// 从 URL 构建 Host 头值 (非默认端口时附加端口号).
inline std::string build_host_header(const urls::url_view& url)
{
    std::string host_value(url.host());
    auto port = url.port_number();
    if (port != 0)
    {
        if ((url.scheme_id() == urls::scheme::https && port != 443)
            || (url.scheme_id() != urls::scheme::https && port != 80))
        {
            host_value += ':';
            host_value += std::to_string(port);
        }
    }
    return host_value;
}

// 从 URL 构建请求目标 (path + query).
inline std::string build_request_target(const urls::url_view& url)
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
void copy_request_headers(http::request<Body>& req, const http::request<http::string_body>& source)
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

} // namespace detail
} // namespace httpc

//
// test_http_details.cpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// 该翻译单元定义测试模块; 程序入口由 Boost.Test 提供.
#define BOOST_TEST_MODULE httpc_details

#include <boost/test/unit_test.hpp>

#include "httpc/detail_http_helpers.hpp"
#include "httpc/httpc.hpp"

#include <boost/url/parse.hpp>

namespace httpc {
namespace detail {
namespace {

urls::url_view parse(const std::string& text)
{
    auto result = urls::parse_uri(text);
    BOOST_REQUIRE(result.has_value());
    return *result;
}

} // namespace

BOOST_AUTO_TEST_SUITE(request_target_suite)

BOOST_AUTO_TEST_CASE(root_path)
{
    BOOST_TEST(build_request_target(parse("http://example.com")) == "/");
    BOOST_TEST(build_request_target(parse("http://example.com/")) == "/");
}

BOOST_AUTO_TEST_CASE(path_and_query)
{
    BOOST_TEST(build_request_target(parse("http://example.com/a/b/c")) == "/a/b/c");
    BOOST_TEST(build_request_target(parse("http://example.com/a/b?x=1&y=2")) == "/a/b?x=1&y=2");
    BOOST_TEST(build_request_target(parse("http://example.com/?q=a%20b")) == "/?q=a%20b");
    BOOST_TEST(build_request_target(parse("http://example.com/a?")) == "/a?");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(host_header_suite)

BOOST_AUTO_TEST_CASE(default_ports_are_omitted)
{
    BOOST_TEST(build_host_header(parse("http://example.com/a")) == "example.com");
    BOOST_TEST(build_host_header(parse("http://example.com:80/a")) == "example.com");
    BOOST_TEST(build_host_header(parse("https://example.com/a")) == "example.com");
    BOOST_TEST(build_host_header(parse("https://example.com:443/a")) == "example.com");
}

BOOST_AUTO_TEST_CASE(non_default_ports_are_kept)
{
    BOOST_TEST(build_host_header(parse("http://example.com:8080/a")) == "example.com:8080");
    BOOST_TEST(build_host_header(parse("https://example.com:8443/a")) == "example.com:8443");
    BOOST_TEST(build_host_header(parse("https://example.com:80/a")) == "example.com:80");
    BOOST_TEST(build_host_header(parse("http://example.com:443/a")) == "example.com:443");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(setup_request_suite)

BOOST_AUTO_TEST_CASE(sets_target_and_host)
{
    http_request req;
    req.method(verb::get);

    setup_request_from_url(req, parse("http://example.com/a/b?c=d"));

    BOOST_TEST(req.target() == "/a/b?c=d");
    BOOST_TEST(req[http::field::host] == "example.com");
}

BOOST_AUTO_TEST_CASE(keeps_user_supplied_host)
{
    http_request req;
    req.set(http::field::host, "custom.example.com");

    setup_request_from_url(req, parse("http://example.com/a"));

    BOOST_TEST(req[http::field::host] == "custom.example.com");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(copy_request_headers_suite)

BOOST_AUTO_TEST_CASE(defaults_are_applied)
{
    http_request source;
    http::request<http::empty_body> target;

    copy_request_headers(target, source);

    BOOST_TEST(target[http::field::user_agent] == default_user_agent);
    BOOST_TEST(target[http::field::connection] == "close");
    BOOST_TEST(target.count(http::field::host) == 0u);
}

BOOST_AUTO_TEST_CASE(source_values_win)
{
    http_request source;
    source.set(http::field::host, "example.com");
    source.set(http::field::user_agent, "unit-test-agent");
    source.set(http::field::connection, "keep-alive");
    source.set(http::field::authorization, "Bearer token");
    source.set("x-custom", "42");

    http::request<http::empty_body> target;
    copy_request_headers(target, source);

    BOOST_TEST(target[http::field::host] == "example.com");
    BOOST_TEST(target[http::field::user_agent] == "unit-test-agent");
    BOOST_TEST(target[http::field::connection] == "keep-alive");
    BOOST_TEST(target[http::field::authorization] == "Bearer token");
    BOOST_TEST(target["x-custom"] == "42");
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace detail
} // namespace httpc

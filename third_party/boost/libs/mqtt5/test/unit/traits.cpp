//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mqtt5/mqtt_client.hpp>

#include <boost/mqtt5/detail/any_authenticator.hpp>
#include <boost/mqtt5/detail/async_traits.hpp>

#include <boost/asio/async_result.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/system_executor.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/type_traits/remove_cv_ref.hpp>

#include <string>
#include <string_view>
#include <type_traits>

#include "test_common/extra_deps.hpp"

using namespace boost::mqtt5;

struct good_authenticator {
    good_authenticator() = default;

    template <typename CompletionToken>
    decltype(auto) async_auth(
        auth_step_e step, std::string data,
        CompletionToken&& token
    ) {
        using error_code = boost::system::error_code;
        using Signature = void (error_code, std::string);

        auto initiate = [](auto, auth_step_e, std::string) {};

        return asio::async_initiate<CompletionToken, Signature>(
            initiate, token, step, std::move(data)
        );
    }

    std::string_view method() const {
        return "method";
    }
};

struct bad_authenticator {
    bad_authenticator() = default;

    void async_auth(std::string /* data */) {}

    std::string_view method() const {
        return "method";
    }
};


BOOST_STATIC_ASSERT(detail::is_authenticator<good_authenticator>);
BOOST_STATIC_ASSERT(!detail::is_authenticator<bad_authenticator>);

namespace asio = boost::asio;

using tcp_layer = asio::ip::tcp::socket;

BOOST_STATIC_ASSERT(!detail::has_next_layer<tcp_layer>);
BOOST_STATIC_ASSERT(!detail::has_tls_layer<tcp_layer>);
BOOST_STATIC_ASSERT(!detail::has_tls_handshake<tcp_layer>);
BOOST_STATIC_ASSERT(!detail::has_ws_handshake<tcp_layer>);
BOOST_STATIC_ASSERT(std::is_same_v<detail::next_layer_type<tcp_layer>, tcp_layer>);
BOOST_STATIC_ASSERT(std::is_same_v<detail::lowest_layer_type<tcp_layer>, tcp_layer>);

void tcp_layers_test() {
    asio::system_executor ex;
    tcp_layer layer(ex);

    detail::next_layer_type<tcp_layer>& nlayer = detail::next_layer(layer);
    BOOST_STATIC_ASSERT(std::is_same_v<boost::remove_cv_ref_t<decltype(nlayer)>, tcp_layer>);

    detail::lowest_layer_type<tcp_layer>& llayer = detail::lowest_layer(layer);
    BOOST_STATIC_ASSERT(std::is_same_v<boost::remove_cv_ref_t<decltype(llayer)>, tcp_layer>);
}

#ifdef BOOST_MQTT5_EXTRA_DEPS

namespace beast = boost::beast;
using tls_layer = asio::ssl::stream<asio::ip::tcp::socket>;
using websocket_tcp_layer = beast::websocket::stream<tcp_layer>;
using websocket_tls_layer = beast::websocket::stream<tls_layer>;

BOOST_STATIC_ASSERT(detail::has_next_layer<tls_layer>);
BOOST_STATIC_ASSERT(detail::has_tls_layer<tls_layer>);
BOOST_STATIC_ASSERT(detail::has_tls_handshake<tls_layer>);
BOOST_STATIC_ASSERT(!detail::has_ws_handshake<tls_layer>);
BOOST_STATIC_ASSERT(std::is_same_v<detail::next_layer_type<tls_layer>, tcp_layer>);
BOOST_STATIC_ASSERT(std::is_same_v<detail::lowest_layer_type<tls_layer>, tcp_layer>);

BOOST_STATIC_ASSERT(detail::has_next_layer<websocket_tcp_layer>);
BOOST_STATIC_ASSERT(!detail::has_tls_layer<websocket_tcp_layer>);
BOOST_STATIC_ASSERT(!detail::has_tls_handshake<websocket_tcp_layer>);
BOOST_STATIC_ASSERT(detail::has_ws_handshake<websocket_tcp_layer>);
BOOST_STATIC_ASSERT(std::is_same_v<detail::next_layer_type<websocket_tcp_layer>, tcp_layer>);
BOOST_STATIC_ASSERT(std::is_same_v<detail::lowest_layer_type<websocket_tcp_layer>, tcp_layer>);

BOOST_STATIC_ASSERT(detail::has_next_layer<websocket_tls_layer>);
BOOST_STATIC_ASSERT(detail::has_tls_layer<websocket_tls_layer>);
BOOST_STATIC_ASSERT(!detail::has_tls_handshake<websocket_tls_layer>);
BOOST_STATIC_ASSERT(detail::has_ws_handshake<websocket_tls_layer>);
BOOST_STATIC_ASSERT(std::is_same_v<detail::next_layer_type<websocket_tls_layer>, tls_layer>);
BOOST_STATIC_ASSERT(std::is_same_v<detail::lowest_layer_type<websocket_tls_layer>, tcp_layer>);

void tls_layers_test() {
    asio::system_executor ex; 
    asio::ssl::context ctx(asio::ssl::context::tls_client);
    tls_layer layer(ex, ctx);

    detail::next_layer_type<tls_layer>& nlayer = detail::next_layer(layer);
    BOOST_STATIC_ASSERT(std::is_same_v<boost::remove_cv_ref_t<decltype(nlayer)>, tcp_layer>);

    detail::lowest_layer_type<tls_layer>& llayer = detail::lowest_layer(layer);
    BOOST_STATIC_ASSERT(std::is_same_v<boost::remove_cv_ref_t<decltype(llayer)>, tcp_layer>);
}

void websocket_tcp_layers_test() {
    asio::system_executor ex;
    websocket_tcp_layer layer(ex);

    detail::next_layer_type<websocket_tcp_layer>& nlayer = detail::next_layer(layer);
    BOOST_STATIC_ASSERT(std::is_same_v<boost::remove_cv_ref_t<decltype(nlayer)>, tcp_layer>);

    detail::lowest_layer_type<websocket_tcp_layer>& llayer = detail::lowest_layer(layer);
    BOOST_STATIC_ASSERT(std::is_same_v<boost::remove_cv_ref_t<decltype(llayer)>, tcp_layer>);
}

void websocket_tls_layers_test() {
    asio::system_executor ex;
    asio::ssl::context ctx(asio::ssl::context::tls_client);
    websocket_tls_layer layer(ex, ctx);

    detail::next_layer_type<websocket_tls_layer>& nlayer = detail::next_layer(layer);
    BOOST_STATIC_ASSERT(std::is_same_v<boost::remove_cv_ref_t<decltype(nlayer)>, tls_layer>);

    detail::lowest_layer_type<websocket_tls_layer>& llayer = detail::lowest_layer(layer);
    BOOST_STATIC_ASSERT(std::is_same_v<boost::remove_cv_ref_t<decltype(llayer)>, tcp_layer>);
}

#endif // BOOST_MQTT5_EXTRA_DEPS

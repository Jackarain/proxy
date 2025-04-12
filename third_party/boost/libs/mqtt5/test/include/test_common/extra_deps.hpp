//
// Copyright (c) 2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_TEST_EXTRA_DEPS_HPP
#define BOOST_MQTT5_TEST_EXTRA_DEPS_HPP

#ifdef BOOST_MQTT5_EXTRA_DEPS
#include <boost/mqtt5/websocket_ssl.hpp>

namespace boost::mqtt5 {

template <typename StreamBase>
struct tls_handshake_type<asio::ssl::stream<StreamBase>> {
    static constexpr auto client = asio::ssl::stream_base::client;
    static constexpr auto server = asio::ssl::stream_base::server;
};

template <typename StreamBase>
void assign_tls_sni(
    const authority_path& ap,
    asio::ssl::context& /* ctx */,
    asio::ssl::stream<StreamBase>& stream
) {
    SSL_set_tlsext_host_name(stream.native_handle(), ap.host.c_str());
}

} // end namespace boost::mqtt5
#endif // BOOST_MQTT5_EXTRA_DEPS

#endif // BOOST_MQTT5_TEST_EXTRA_DEPS_HPP

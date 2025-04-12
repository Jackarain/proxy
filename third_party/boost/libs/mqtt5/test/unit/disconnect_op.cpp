//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mqtt5/types.hpp>

#include <boost/mqtt5/impl/disconnect_op.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include "test_common/test_service.hpp"

using namespace boost::mqtt5;

BOOST_AUTO_TEST_SUITE(disconnect_op/*, *boost::unit_test::disabled()*/)

void run_malformed_props_test(const disconnect_props& dprops) {
    constexpr int expected_handlers_called = 1;
    int handlers_called = 0;

    asio::io_context ioc;
    using client_service_type = test::test_service<asio::ip::tcp::socket>;
    auto svc_ptr = std::make_shared<client_service_type>(ioc.get_executor());

    auto handler = [&handlers_called](error_code ec) {
        ++handlers_called;
        BOOST_TEST(ec == client::error::malformed_packet);
    };

    detail::disconnect_ctx ctx;
    ctx.props = dprops;

    detail::disconnect_op<
        client_service_type, detail::disconnect_ctx
    > { svc_ptr, std::move(ctx), std::move(handler) }
    .perform();

    ioc.run_for(std::chrono::milliseconds(500));
    BOOST_TEST(handlers_called == expected_handlers_called);
}

BOOST_AUTO_TEST_CASE(malformed_reason_string) {
    disconnect_props dprops;
    dprops[prop::reason_string] = std::string { 0x01 };

    run_malformed_props_test(dprops);
}

BOOST_AUTO_TEST_CASE(malformed_user_property_key) {
    disconnect_props dprops;
    dprops[prop::user_property].emplace_back(std::string { 0x01 }, "value");

    run_malformed_props_test(dprops);
}

BOOST_AUTO_TEST_CASE(malformed_user_property_value) {
    disconnect_props dprops;
    dprops[prop::user_property].emplace_back("key", std::string { 0x01 });

    run_malformed_props_test(dprops);
}

BOOST_AUTO_TEST_SUITE_END()

//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mqtt5/logger_traits.hpp>

#include <boost/mqtt5/detail/log_invoke.hpp>

#include <boost/mqtt5/impl/client_service.hpp>
#include <boost/mqtt5/impl/reconnect_op.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/prepend.hpp>
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <memory>

#include "test_common/test_autoconnect_stream.hpp"
#include "test_common/test_broker.hpp"
#include "test_common/test_stream.hpp"

using namespace boost::mqtt5;
using namespace std::chrono_literals;

BOOST_AUTO_TEST_SUITE(reconnect_op/*, *boost::unit_test::disabled()*/)

BOOST_AUTO_TEST_CASE(exponential_backoff) {
    detail::exponential_backoff generator;

    auto first_iter = generator.generate();
    BOOST_TEST((first_iter >= 500ms && first_iter <= 1500ms));

    auto second_iter = generator.generate();
    BOOST_TEST((second_iter >= 1500ms && first_iter <= 2500ms));

    auto third_iter = generator.generate();
    BOOST_TEST((third_iter >= 3500ms && third_iter <= 4500ms));

    auto fourth_iter = generator.generate();
    BOOST_TEST((fourth_iter >= 7500ms && fourth_iter <= 8500ms));

    auto fifth_iter = generator.generate();
    BOOST_TEST((fifth_iter >= 15500ms && fourth_iter <= 16500ms));

    auto sixth_iter = generator.generate();
    BOOST_TEST((sixth_iter >= 15500ms && sixth_iter <= 16500ms));
}

struct test_tcp_stream : public test::test_stream {

    test_tcp_stream(
        typename test::test_stream::executor_type ex
    ) :
        test::test_stream(std::move(ex))
    {}

    static int& succeed_after() {
        static int _succed_after = 0;
        return _succed_after;
    }

    template <typename ConnectToken>
    decltype(auto) async_connect(
        const endpoint_type& ep, ConnectToken&& token
    ) {
        auto initiation = [this](auto handler, const endpoint_type& ep) {
            error_code ec = --succeed_after() < 0 ? error_code {} : asio::error::connection_refused;
            if (!ec) {
                error_code cec;
                test::test_stream::open(ep.protocol(), cec);
                test::test_stream::connect(ep, cec);
            }
            asio::post(get_executor(), asio::prepend(std::move(handler), ec));
        };

        return asio::async_initiate<ConnectToken, void(error_code)>(
            std::move(initiation), token, ep
        );
    }
};

template <typename ShutdownHandler>
void async_shutdown(test_tcp_stream&, ShutdownHandler&& handler) {
    return std::move(handler)(error_code {});
}

using underlying_stream = test_tcp_stream;
using stream_context = detail::stream_context<underlying_stream, std::monostate>;
using astream = test::test_autoconnect_stream<underlying_stream, stream_context>;

void run_connect_to_localhost_test(int succeed_after) {
    using test::after;
    error_code success {};

    const std::string connect = encoders::encode_connect(
        "", std::nullopt, std::nullopt, 60, false, {}, std::nullopt
    );
    const std::string connack = encoders::encode_connack(
        true, reason_codes::success.value(), {}
    );

    constexpr int expected_handlers_called = 1;
    int handlers_called = 0;

    asio::io_context ioc;
    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(2ms))
            .reply_with(connack, after(4ms));

    auto& broker = asio::make_service<test::test_broker>(
        ioc, ioc.get_executor(), std::move(broker_side)
    );

    auto stream_ctx = stream_context(std::monostate {});
    auto log = detail::log_invoke<noop_logger>();
    auto auto_stream = astream(ioc.get_executor(), stream_ctx, log);
    auto_stream.brokers("localhost", 1883);

    auto handler = [&handlers_called](error_code ec) {
        ++handlers_called;
        BOOST_TEST(!ec);
    };

    test_tcp_stream::succeed_after() = succeed_after;
    detail::reconnect_op(auto_stream, std::move(handler))
        .perform(auto_stream.stream_pointer());

    ioc.run();
    BOOST_TEST(expected_handlers_called == handlers_called);
    BOOST_TEST(broker.received_all_expected());
}

BOOST_AUTO_TEST_CASE(connect_to_first_localhost) {
    // connect to first in the resolver list
    run_connect_to_localhost_test(2);
}

BOOST_AUTO_TEST_CASE(connect_to_second_localhost) {
    // connect to second in the resolver list
    run_connect_to_localhost_test(3);
}

BOOST_AUTO_TEST_CASE(no_servers) {
    constexpr int expected_handlers_called = 1;
    int handlers_called = 0;

    asio::io_context ioc;
    auto stream_ctx = stream_context(std::monostate{});
    auto log = detail::log_invoke<noop_logger>();
    auto auto_stream = astream(ioc.get_executor(), stream_ctx, log);
    auto_stream.brokers("", 1883);

    auto handler = [&handlers_called](error_code ec) {
        ++handlers_called;
        BOOST_TEST(ec == asio::error::no_recovery);
    };

    detail::reconnect_op(auto_stream, std::move(handler))
        .perform(auto_stream.stream_pointer());

    ioc.run();
    BOOST_TEST(expected_handlers_called == handlers_called);
}

BOOST_AUTO_TEST_SUITE_END();

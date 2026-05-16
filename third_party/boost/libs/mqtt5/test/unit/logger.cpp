//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "test_common/message_exchange.hpp"
#include "test_common/test_stream.hpp"

#include <boost/mqtt5/logger.hpp>
#include <boost/mqtt5/logger_traits.hpp>
#include <boost/mqtt5/mqtt_client.hpp>

#include <boost/mqtt5/detail/log_invoke.hpp>

#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>
#include <boost/test/tools/output_test_stream.hpp>
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>

using namespace boost::mqtt5;
namespace asio = boost::asio;

void logger_test() {
    BOOST_STATIC_ASSERT(has_at_resolve<logger>);
    BOOST_STATIC_ASSERT(has_at_tcp_connect<logger>);
    BOOST_STATIC_ASSERT(has_at_tls_handshake<logger>);
    BOOST_STATIC_ASSERT(has_at_ws_handshake<logger>);
    BOOST_STATIC_ASSERT(has_at_connack<logger>);
    BOOST_STATIC_ASSERT(has_at_disconnect<logger>);
    BOOST_STATIC_ASSERT(has_at_transport_error<logger>);
}

BOOST_AUTO_TEST_SUITE(logger_tests)

using error_code = boost::system::error_code;

class clog_redirect {
    std::streambuf* _old_buffer;

public:
    clog_redirect(std::streambuf* new_buffer) :
        _old_buffer(std::clog.rdbuf(new_buffer))
    {}

    ~clog_redirect() {
        std::clog.rdbuf(_old_buffer);
    }

};

bool contains(const std::string& str, const std::string& substr) {
    return str.find(substr) != std::string::npos;
}

// Error code messages differ from platform to platform.

std::string success_msg() {
    error_code ec;
    return ec.message();
}

std::string host_not_found_msg() {
    error_code ec = asio::error::host_not_found;
    return ec.message();
}

std::string not_connected_msg() {
    error_code ec = asio::error::not_connected;
    return ec.message();
}

// Test logger output based on log level.

template <typename TestFun>
void test_logger_output(TestFun&& fun, const std::string& expected_output) {
    boost::test_tools::output_test_stream output;
    {
        clog_redirect guard(output.rdbuf());
        fun();
    }

    auto realized_output = output.rdbuf()->str();
    BOOST_TEST(realized_output == expected_output);
}

// at_resolve

struct resolve_test_data {
    std::string_view host = "localhost";
    std::string_view port = "1883";

    auto endpoints() {
        return asio::ip::tcp::resolver::results_type::create(
            { asio::ip::make_address("127.0.0.1"), 1883 },
            std::string(host), std::string(port)
        );
    }
};

BOOST_FIXTURE_TEST_CASE(at_resolve_success_warning, resolve_test_data) {
    const auto expected_output = "";

    auto test_fun = [this] {
        logger l(log_level::warning);
        l.at_resolve(error_code {}, host, port, endpoints());
    };

    test_logger_output(std::move(test_fun), expected_output);
}

BOOST_FIXTURE_TEST_CASE(at_resolve_fail_warning, resolve_test_data) {
    const auto expected_output =
        "[Boost.MQTT5] resolve: localhost:1883 - " + host_not_found_msg() + ".\n"
    ;

    auto test_fun = [this] {
        logger l(log_level::warning);
        l.at_resolve(asio::error::host_not_found, host, port, {});
    };

    test_logger_output(std::move(test_fun), expected_output);
}

BOOST_FIXTURE_TEST_CASE(at_resolve_info, resolve_test_data) {
    const auto expected_output = 
        "[Boost.MQTT5] resolve: localhost:1883 - " + success_msg() + ".\n"
    ;

    auto test_fun = [this] {
        logger l(log_level::info);
        l.at_resolve(error_code {}, host, port, endpoints());
    };

    test_logger_output(std::move(test_fun), expected_output);
}

BOOST_FIXTURE_TEST_CASE(at_resolve_success_debug, resolve_test_data) {
    const auto expected_output = 
        "[Boost.MQTT5] resolve: localhost:1883 - " + success_msg() + ". [127.0.0.1]\n"
    ;

    auto test_fun = [this] {
        logger l(log_level::debug);
        l.at_resolve(error_code {}, host, port, endpoints());
    };

    test_logger_output(std::move(test_fun), expected_output);
}

BOOST_FIXTURE_TEST_CASE(at_resolve_fail_debug, resolve_test_data) {
    const auto expected_output = 
        "[Boost.MQTT5] resolve: localhost:1883 - " + host_not_found_msg() + ". []\n"
    ;

    auto test_fun = [this] {
        logger l(log_level::debug);
        l.at_resolve(asio::error::host_not_found, host, port, {});
    };

    test_logger_output(std::move(test_fun), expected_output);
}

struct tcp_endpoint {
    const asio::ip::tcp::endpoint ep = { asio::ip::make_address_v4("127.0.0.1"), 1883 };

    std::string endpoint_output() {
        return ep.address().to_string() + ":" + std::to_string(ep.port());
    }
};

// at_tcp_connect

BOOST_FIXTURE_TEST_CASE(at_tcp_connect_success_warning, tcp_endpoint) {
    const auto expected_output = "";

    auto test_fun = [this] {
        logger l(log_level::warning);
        l.at_tcp_connect(error_code {}, ep);
    };

    test_logger_output(std::move(test_fun), expected_output);
}

BOOST_FIXTURE_TEST_CASE(at_tcp_connect_fail_warning, tcp_endpoint) {
    const auto expected_output =
        "[Boost.MQTT5] TCP connect: " + endpoint_output() + " - " + not_connected_msg() + ".\n"
    ;

    auto test_fun = [this] {
        logger l(log_level::warning);
        l.at_tcp_connect(asio::error::not_connected, ep);
    };

    test_logger_output(std::move(test_fun), expected_output);
}

BOOST_FIXTURE_TEST_CASE(at_tcp_connect_info, tcp_endpoint) {
    const auto expected_output =
        "[Boost.MQTT5] TCP connect: " + endpoint_output() + " - " + success_msg() + ".\n"
    ;

    auto test_fun = [this] {
        logger l(log_level::info);
        l.at_tcp_connect(error_code {}, ep);
    };

    test_logger_output(std::move(test_fun), expected_output);
}

// at_tls_handshake

BOOST_FIXTURE_TEST_CASE(at_tls_handshake_success_warning, tcp_endpoint) {
    const auto expected_output = "";

    auto test_fun = [this] {
        logger l(log_level::warning);
        l.at_tls_handshake(error_code {}, ep);
    };

    test_logger_output(std::move(test_fun), expected_output);
}

BOOST_FIXTURE_TEST_CASE(at_tls_handshake_fail_warning, tcp_endpoint) {
    const auto expected_output =
        "[Boost.MQTT5] TLS handshake: " + endpoint_output() + " - " + not_connected_msg() + ".\n"
    ;

    auto test_fun = [this] {
        logger l(log_level::warning);
        l.at_tls_handshake(asio::error::not_connected, ep);
    };

    test_logger_output(std::move(test_fun), expected_output);
}

BOOST_FIXTURE_TEST_CASE(at_tls_handshake_info, tcp_endpoint) {
    const auto expected_output =
        "[Boost.MQTT5] TLS handshake: " + endpoint_output() + " - " + success_msg() + ".\n"
    ;

    auto test_fun = [this] {
        logger l(log_level::info);
        l.at_tls_handshake(error_code {}, ep);
    };

    test_logger_output(std::move(test_fun), expected_output);
}

// at_ws_handshake

BOOST_FIXTURE_TEST_CASE(at_ws_handshake_success_warning, tcp_endpoint) {
    const auto expected_output = "";

    auto test_fun = [this] {
        logger l(log_level::warning);
        l.at_ws_handshake(error_code {}, ep);
    };

    test_logger_output(std::move(test_fun), expected_output);
}

BOOST_FIXTURE_TEST_CASE(at_ws_handshake_fail_warning, tcp_endpoint) {
    const auto expected_output =
        "[Boost.MQTT5] WebSocket handshake: " + endpoint_output() + " - " + not_connected_msg() + ".\n"
    ;

    auto test_fun = [this] {
        logger l(log_level::warning);
        l.at_ws_handshake(asio::error::not_connected, ep);
    };

    test_logger_output(std::move(test_fun), expected_output);
}

BOOST_FIXTURE_TEST_CASE(at_ws_handshake_info, tcp_endpoint) {
    const auto expected_output =
        "[Boost.MQTT5] WebSocket handshake: " + endpoint_output() + " - " + success_msg() + ".\n"
    ;

    auto test_fun = [this] {
        logger l(log_level::info);
        l.at_ws_handshake(error_code {}, ep);
    };

    test_logger_output(std::move(test_fun), expected_output);
}

// at_connack

struct connack_test_data {
    connack_props ca_props() {
        connack_props cprops;
        cprops[prop::user_property] = { { "key_1", "val_1" }, {"key_2", "val_2"} };;
        cprops[prop::subscription_identifier_available] = uint8_t(1);
        cprops[prop::assigned_client_identifier] = "client_id";
        return cprops;
    }
};

BOOST_FIXTURE_TEST_CASE(at_connack_success_warning, connack_test_data) {
    const auto expected_output = "";

    auto test_fun = [this] {
        logger l(log_level::warning);
        l.at_connack(reason_codes::success, false, ca_props());
    };

    test_logger_output(std::move(test_fun), expected_output);
}

BOOST_FIXTURE_TEST_CASE(at_connack_fail_warning, connack_test_data) {
    const auto expected_output =
        "[Boost.MQTT5] connack: The Client ID is valid but not allowed by this Server.\n"
    ;

    auto test_fun = [this] {
        logger l(log_level::warning);
        l.at_connack(reason_codes::client_identifier_not_valid, false, ca_props());
    };

    test_logger_output(std::move(test_fun), expected_output);
}

BOOST_FIXTURE_TEST_CASE(at_connack_info, connack_test_data) {
    const auto expected_output =
        "[Boost.MQTT5] connack: The operation completed successfully.\n"
    ;

    auto test_fun = [this] {
        logger l(log_level::info);
        l.at_connack(reason_codes::success, false, ca_props());
    };

    test_logger_output(std::move(test_fun), expected_output);
}

BOOST_FIXTURE_TEST_CASE(at_connack_debug, connack_test_data) {
    const auto expected_output =
        "[Boost.MQTT5] connack: The operation completed successfully. "
        "session_present:0 "
        "assigned_client_identifier:client_id "
        "user_property:[(key_1,val_1), (key_2,val_2)] "
        "subscription_identifier_available:1 \n"
    ;

    auto test_fun = [this] {
        logger l(log_level::debug);
        l.at_connack(reason_codes::success, false, ca_props());
    };

    test_logger_output(std::move(test_fun), expected_output);
}

// at_disconnect

struct disconnect_test_data {
    disconnect_props dc_props() {
        disconnect_props dprops;
        dprops[prop::reason_string] = "No reason.";
        return dprops;
    }
};

BOOST_FIXTURE_TEST_CASE(at_disconnect_info, disconnect_test_data) {
    const auto expected_output =
        "[Boost.MQTT5] disconnect: Close the connection normally. "
        "Do not send the Will Message.\n"
    ;

    auto test_fun = [this] {
        logger l(log_level::info);
        l.at_disconnect(reason_codes::normal_disconnection, dc_props());
    };

    test_logger_output(std::move(test_fun), expected_output);
}

BOOST_FIXTURE_TEST_CASE(at_disconnect_debug, disconnect_test_data) {
    const auto expected_output =
        "[Boost.MQTT5] disconnect: Close the connection normally. "
        "Do not send the Will Message.reason_string:No reason. \n"
    ;

    auto test_fun = [this] {
        logger l(log_level::debug);
        l.at_disconnect(reason_codes::normal_disconnection, dc_props());
    };

    test_logger_output(std::move(test_fun), expected_output);
}

// at_transport_error

BOOST_AUTO_TEST_CASE(at_transport_error_info) {
    const auto expected_output = "[Boost.MQTT5] transport layer error: End of file.\n";

    auto test_fun = [] {
        logger l(log_level::info);
        l.at_transport_error(asio::error::eof);
    };

    test_logger_output(std::move(test_fun), expected_output);
}

// Test that the mqtt_client calls logger functions as expected.

BOOST_AUTO_TEST_CASE(client_disconnect) {
    using test::after;
    using namespace std::chrono_literals;

    const auto& success = success_msg();
    const auto expected_msg =
        "[Boost.MQTT5] resolve: 127.0.0.1:1883 - " + success + ".\n"
        "[Boost.MQTT5] TCP connect: 127.0.0.1:1883 - " + success + ".\n"
        "[Boost.MQTT5] connack: The operation completed successfully.\n"
        "[Boost.MQTT5] disconnect: Close the connection normally. Do not send the Will Message.\n"
        "[Boost.MQTT5] resolve: 127.0.0.1:1883 - " + success + ".\n"
        "[Boost.MQTT5] TCP connect: 127.0.0.1:1883 - " + success + ".\n"
    ;

    boost::test_tools::output_test_stream output;
    {
        clog_redirect guard(output.rdbuf());

        // packets
        auto connect = encoders::encode_connect(
            "", std::nullopt, std::nullopt, 60, false, test::dflt_cprops, std::nullopt
        );
        auto connack = encoders::encode_connack(false, uint8_t(0x00), {});

        disconnect_props dc_props;
        dc_props[prop::reason_string] = "No reason.";
        auto disconnect = encoders::encode_disconnect(0x00, dc_props);

        test::msg_exchange broker_side;
        broker_side
            .expect(connect)
                .complete_with(error_code {}, after(0ms))
                .reply_with(connack, after(0ms))
            .send(disconnect, after(30ms))
            .expect(connect);

        asio::io_context ioc;
        auto executor = ioc.get_executor();
        auto& broker = asio::make_service<test::test_broker>(
            ioc, executor, std::move(broker_side)
        );

        mqtt_client<test::test_stream, std::monostate, logger> c(executor);
        c.brokers("127.0.0.1,127.0.0.1") // to avoid reconnect backoff
            .async_run(asio::detached);

        test::test_timer timer(c.get_executor());
        timer.expires_after(100ms);
        timer.async_wait([&c](error_code) { c.cancel(); });

        broker.run(ioc);
        BOOST_TEST(broker.received_all_expected());
    }

    std::string log = output.rdbuf()->str();
    BOOST_TEST(log == expected_msg);
}

BOOST_AUTO_TEST_CASE(client_transport_error) {
    using test::after;
    using namespace std::chrono_literals;

    const auto& success = success_msg();
    const auto expected_msg =
        "[Boost.MQTT5] resolve: 127.0.0.1:1883 - " + success + ".\n"
        "[Boost.MQTT5] TCP connect: 127.0.0.1:1883 - " + success + ".\n"
        "[Boost.MQTT5] transport layer error: End of file.\n"
        "[Boost.MQTT5] resolve: 127.0.0.1:1883 - " + success + ".\n"
        "[Boost.MQTT5] TCP connect: 127.0.0.1:1883 - " + success + ".\n"
        "[Boost.MQTT5] connack: The operation completed successfully.\n"
    ;

    boost::test_tools::output_test_stream output;
    {
        clog_redirect guard(output.rdbuf());

        // packets
        auto connect = encoders::encode_connect(
            "", std::nullopt, std::nullopt, 60, false, test::dflt_cprops, std::nullopt
        );
        auto connack = encoders::encode_connack(false, uint8_t(0x00), {});

        const std::string topic = "topic", payload = "payload0";
        const std::string publish = encoders::encode_publish(
            0, topic, payload, qos_e::at_most_once, retain_e::no, dup_e::no, {}
        );

        test::msg_exchange broker_side;
        broker_side
            .expect(connect)
                .complete_with(asio::error::eof, after(0ms))
            .expect(connect)
                .complete_with(error_code{}, after(0ms))
                .reply_with(connack, after(10ms));

        asio::io_context ioc;
        auto executor = ioc.get_executor();
        auto& broker = asio::make_service<test::test_broker>(
            ioc, executor, std::move(broker_side)
        );

        mqtt_client<test::test_stream, std::monostate, logger> c(executor);
        c.brokers("127.0.0.1,127.0.0.1") // to avoid reconnect backoff
            .async_run(asio::detached);

        test::test_timer timer(c.get_executor());
        timer.expires_after(100ms);
        timer.async_wait([&c](error_code) { c.cancel(); });

        broker.run(ioc);
        BOOST_TEST(broker.received_all_expected());
    }

    std::string log = output.rdbuf()->str();
    BOOST_TEST(log == expected_msg);
}

BOOST_AUTO_TEST_SUITE_END();

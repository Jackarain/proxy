//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

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

#include "test_common/extra_deps.hpp"
#include "test_common/message_exchange.hpp"
#include "test_common/preconditions.hpp"
#include "test_common/test_service.hpp"
#include "test_common/test_stream.hpp"

using namespace boost::mqtt5;
namespace asio = boost::asio;

void logger_test() {
    BOOST_STATIC_ASSERT(has_at_resolve<logger>);
    BOOST_STATIC_ASSERT(has_at_tcp_connect<logger>);
    BOOST_STATIC_ASSERT(has_at_tls_handshake<logger>);
    BOOST_STATIC_ASSERT(has_at_ws_handshake<logger>);
    BOOST_STATIC_ASSERT(has_at_connack<logger>);
    BOOST_STATIC_ASSERT(has_at_disconnect<logger>);
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

    asio::ip::tcp::resolver::results_type endpoints() {
        auto ex = asio::system_executor {};
        asio::ip::tcp::resolver resolver(ex);
        error_code ec;
        auto eps = resolver.resolve(host, port, ec);
        BOOST_TEST_REQUIRE(!ec);
        return eps;
    }

    std::string endpoints_output() {
        // Endpoints resolved depend on the platform.
        auto eps = endpoints();
        std::stringstream ss;
        ss << "[";
        for (auto it = eps.begin(); it != eps.end();) {
           ss << it->endpoint().address().to_string();
            if (++it != eps.end())
                ss << ",";
        }
        ss << "]";
        return ss.str();
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
        "[Boost.MQTT5] resolve: localhost:1883 - " + success_msg() + ". " + endpoints_output() + "\n"
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
            "", std::nullopt, std::nullopt, 60, false, {}, std::nullopt
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
            .send(disconnect, after(50ms))
            .expect(connect);

        asio::io_context ioc;
        auto executor = ioc.get_executor();
        auto& broker = asio::make_service<test::test_broker>(
            ioc, executor, std::move(broker_side)
        );

        mqtt_client<test::test_stream, std::monostate, logger> c(executor);
        c.brokers("127.0.0.1,127.0.0.1") // to avoid reconnect backoff
            .async_run(asio::detached);

        asio::steady_timer timer(c.get_executor());
        timer.expires_after(100ms);
        timer.async_wait([&c](error_code) { c.cancel(); });

        ioc.run();
        BOOST_TEST(broker.received_all_expected());
    }

    std::string log = output.rdbuf()->str();
    BOOST_TEST(log == expected_msg);
}

#ifdef BOOST_MQTT5_EXTRA_DEPS
using stream_type = boost::beast::websocket::stream<
    asio::ssl::stream<asio::ip::tcp::socket>
>;
using context_type = asio::ssl::context;
using logger_type = logger;
using client_type = mqtt_client<stream_type, context_type, logger_type>;

BOOST_AUTO_TEST_CASE(client_successful_connect_debug,
    * boost::unit_test::precondition(test::public_broker_cond))
{
    boost::test_tools::output_test_stream output;

    {
        clog_redirect guard(output.rdbuf());
        asio::io_context ioc;

        asio::ssl::context tls_context(asio::ssl::context::tls_client);
        client_type c(
            ioc, std::move(tls_context), logger(log_level::debug)
        );

        c.brokers("broker.hivemq.com/mqtt", 8884)
            .async_run(asio::detached);

        c.async_disconnect([](error_code) {});

        ioc.run();
    }

    std::string log = output.rdbuf()->str();
    BOOST_TEST_MESSAGE(log);
    BOOST_TEST_WARN(contains(log, "resolve"));
    BOOST_TEST_WARN(contains(log, "TCP connect"));
    BOOST_TEST_WARN(contains(log, "TLS handshake"));
    BOOST_TEST_WARN(contains(log, "WebSocket handshake"));
    BOOST_TEST_WARN(contains(log, "connack"));
}

BOOST_AUTO_TEST_CASE(client_successful_connect_warning,
    * boost::unit_test::precondition(test::public_broker_cond))
{
    boost::test_tools::output_test_stream output;

    {
        clog_redirect guard(output.rdbuf());

        asio::io_context ioc;
        asio::ssl::context tls_context(asio::ssl::context::tls_client);
        client_type c(
            ioc, std::move(tls_context), logger(log_level::warning)
        );

        c.brokers("broker.hivemq.com/mqtt", 8884)
            .async_run(asio::detached);

        c.async_disconnect([](error_code) {});

        ioc.run();
    }

    // If connection is successful, nothing should be printed.
    // However if the Broker is down or overloaded, this will cause logs to be printed.
    // We should not fail the test because of it.
    BOOST_TEST_WARN(output.is_empty());
}
#endif // BOOST_MQTT5_EXTRA_DEPS

BOOST_AUTO_TEST_SUITE_END();

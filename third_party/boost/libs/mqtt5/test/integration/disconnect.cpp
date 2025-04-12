//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mqtt5/mqtt_client.hpp>
#include <boost/mqtt5/types.hpp>

#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <string>

#include "test_common/message_exchange.hpp"
#include "test_common/test_broker.hpp"
#include "test_common/test_stream.hpp"

using namespace boost::mqtt5;

BOOST_AUTO_TEST_SUITE(disconnect/*, *boost::unit_test::disabled()*/)

struct shared_test_data {
    error_code success {};
    error_code fail = asio::error::not_connected;

    const std::string connect = encoders::encode_connect(
        "", std::nullopt, std::nullopt, 60, false, {}, std::nullopt
    );
    const std::string connack = encoders::encode_connack(
        true, reason_codes::success.value(), {}
    );
    const std::string disconnect = encoders::encode_disconnect(
        reason_codes::normal_disconnection.value(), disconnect_props {}
    );
};

using test::after;
using namespace std::chrono_literals;
using client_type = mqtt_client<test::test_stream>;

template <typename TestCase>
void run_test(test::msg_exchange broker_side, TestCase&& test_case) {
    asio::io_context ioc;
    auto executor = ioc.get_executor();
    auto& broker = asio::make_service<test::test_broker>(
        ioc, executor, std::move(broker_side)
    );

    asio::steady_timer timer(executor);
    client_type c(executor);
    c.brokers("127.0.0.1,127.0.0.1") // to avoid reconnect backoff
        .async_run(asio::detached);

    test_case(c, timer);

    ioc.run();
    BOOST_TEST(broker.received_all_expected());
}

BOOST_FIXTURE_TEST_CASE(successful_disconnect, shared_test_data) {
    constexpr int expected_handlers_called = 1;
    int handlers_called = 0;

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms))
        .expect(disconnect)
            .complete_with(success, after(0ms));

    run_test(
        std::move(broker_side),
        [&](client_type& c, asio::steady_timer& timer) {
            timer.expires_after(100ms);
            timer.async_wait([&](error_code) {
                c.async_disconnect(
                    [&](error_code ec) {
                        handlers_called++;
                        BOOST_TEST(!ec);
                    }
                );
            });
        }
    );

    BOOST_TEST(handlers_called == expected_handlers_called);
}

BOOST_FIXTURE_TEST_CASE(successful_disconnect_in_queue, shared_test_data) {
    constexpr int expected_handlers_called = 2;
    int handlers_called = 0;

    // packets
    auto publish_qos0 = encoders::encode_publish(
        0, "topic", "payload", qos_e::at_most_once, retain_e::no, dup_e::no, {}
    );

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(1ms))
            .reply_with(connack, after(2ms))
        .expect(publish_qos0)
            .complete_with(success, after(1ms))
        .expect(disconnect)
            .complete_with(success, after(0ms));

    run_test(
        std::move(broker_side),
        [&](client_type& c, asio::steady_timer& timer) {
            timer.expires_after(50ms);
            timer.async_wait([&](error_code) {
                c.async_publish<qos_e::at_most_once>(
                    "topic", "payload", retain_e::no, {},
                    [&handlers_called](error_code ec) {
                        BOOST_TEST(handlers_called == 0);
                        handlers_called++;
                        BOOST_TEST(!ec);
                    }
                );
                c.async_disconnect(
                    [&](error_code ec) {
                        BOOST_TEST(handlers_called == 1);
                        handlers_called++;
                        BOOST_TEST(!ec);
                    }
                );
            });
        }
    );

    BOOST_TEST(handlers_called == expected_handlers_called);
}

BOOST_FIXTURE_TEST_CASE(disconnect_on_disconnected_client, shared_test_data) {
    constexpr int expected_handlers_called = 1;
    int handlers_called = 0;

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
        .expect(connect);

    run_test(
        std::move(broker_side),
        [&](client_type& c, asio::steady_timer& timer) {
            timer.expires_after(50ms);
            timer.async_wait([&](error_code) {
                c.async_disconnect(
                    [&](error_code ec) {
                        handlers_called++;
                        BOOST_TEST(ec == asio::error::operation_aborted);
                    }
                );
            });
        }
    );

    BOOST_TEST(handlers_called == expected_handlers_called);
}

BOOST_FIXTURE_TEST_CASE(disconnect_in_queue_on_disconnected_client, shared_test_data) {
    constexpr int expected_handlers_called = 2;
    int handlers_called = 0;

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
        .expect(connect);

    run_test(
        std::move(broker_side),
        [&](client_type& c, asio::steady_timer& timer) {
            timer.expires_after(50ms);
            timer.async_wait([&](error_code) {
                c.async_publish<qos_e::at_most_once>(
                    "topic", "payload", retain_e::no, {},
                    [&handlers_called](error_code ec) {
                        BOOST_TEST(handlers_called == 1);
                        handlers_called++;
                        BOOST_TEST(ec == asio::error::operation_aborted);
                    }
                );
                c.async_disconnect(
                    [&](error_code ec) {
                        BOOST_TEST(handlers_called == 0);
                        handlers_called++;
                        BOOST_TEST(ec == asio::error::operation_aborted);
                    }
                );
            });
        }
    );

    BOOST_TEST(handlers_called == expected_handlers_called);
}

BOOST_FIXTURE_TEST_CASE(resend_terminal_disconnect, shared_test_data) {
    constexpr int expected_handlers_called = 1;
    int handlers_called = 0;

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms))
        .expect(disconnect)
            .complete_with(success, after(0ms));

    run_test(
        std::move(broker_side),
        [&](client_type& c, asio::steady_timer&) {
            c.async_disconnect(
                [&](error_code ec) {
                    handlers_called++;
                    BOOST_TEST(!ec);
                }
            );
        }
    );

    BOOST_TEST(handlers_called == expected_handlers_called);
}

BOOST_FIXTURE_TEST_CASE(dont_resend_non_terminal_disconnect, shared_test_data) {
    auto malformed_publish_1 = encoders::encode_publish(
        1, "malformed topic", "malformed payload",
        static_cast<qos_e>(0b11), retain_e::yes, dup_e::no, {}
    );
    auto malformed_publish_2 = encoders::encode_publish(
        2, "malformed topic", "malformed payload",
        static_cast<qos_e>(0b11), retain_e::yes, dup_e::no, {}
    );

    auto disconnect_malformed_publish = encoders::encode_disconnect(
        reason_codes::malformed_packet.value(),
        test::dprops_with_reason_string("Malformed PUBLISH received: QoS bits set to 0b11")
    );

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms))
        .send(malformed_publish_1, malformed_publish_2, after(10ms))
        .expect(disconnect_malformed_publish)
            .complete_with(success, after(0ms))
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms));

    run_test(
        std::move(broker_side),
        [&](client_type& c, asio::steady_timer& timer) {
            timer.expires_after(50ms);
            timer.async_wait([&](error_code) {
                c.cancel();
            });
        }
    );
}

BOOST_FIXTURE_TEST_CASE(omit_props, shared_test_data) {
    constexpr int expected_handlers_called = 1;
    int handlers_called = 0;

    connack_props co_props;
    co_props[prop::maximum_packet_size] = 20;

    // packets
    auto connack_with_max_packet = encoders::encode_connack(
        false, reason_codes::success.value(), co_props
    );

    disconnect_props props;
    props[prop::reason_string] = std::string(50, 'a');
    auto big_disconnect = encoders::encode_disconnect(
        reason_codes::normal_disconnection.value(), props
    );

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack_with_max_packet, after(0ms))
        .expect(disconnect)
            .complete_with(success, after(0ms));

    run_test(
        std::move(broker_side),
        [&](client_type& c, asio::steady_timer& timer) {
            timer.expires_after(50ms);
            timer.async_wait([&](error_code) {
                c.async_disconnect(
                    disconnect_rc_e::normal_disconnection, props,
                    [&](error_code ec) {
                        handlers_called++;
                        BOOST_TEST(!ec);
                    }
                );
            });
        }
    );

    BOOST_TEST(handlers_called == expected_handlers_called);
}

struct long_shutdown_stream : public test::test_stream {
    long_shutdown_stream(typename test::test_stream::executor_type ex) :
        test::test_stream(std::move(ex)) {}
};

template <typename ShutdownHandler>
void async_shutdown(long_shutdown_stream& stream, ShutdownHandler&& handler) {
    auto timer = std::make_shared<asio::steady_timer>(stream.get_executor());
    timer->expires_after(std::chrono::seconds(10));
    timer->async_wait(asio::consign(std::move(handler), std::move(timer)));
}

BOOST_DATA_TEST_CASE_F(
    shared_test_data, cancel_disconnect_in_shutdown,
    boost::unit_test::data::make({ 100, 8000 }), cancel_delay_ms
) {
    asio::io_context ioc;
    auto executor = ioc.get_executor();

    constexpr int expected_handlers_called = 1;
    int handlers_called = 0;

    test::msg_exchange broker_side;
    broker_side
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms))
        .expect(disconnect)
            .complete_with(success, after(0ms));

    auto& broker = asio::make_service<test::test_broker>(
        ioc, executor, std::move(broker_side)
    );

    asio::steady_timer timer(executor);
    mqtt_client<long_shutdown_stream> c(executor);
    c.brokers("127.0.0.1")
        .async_run(asio::detached);

    asio::cancellation_signal signal;

    c.async_disconnect(
        asio::bind_cancellation_slot(
            signal.slot(),
            [&](error_code ec) {
                handlers_called++;
                BOOST_TEST(ec == asio::error::operation_aborted);
                timer.cancel();
            }
        )
    );

    timer.expires_after(std::chrono::milliseconds(cancel_delay_ms));
    timer.async_wait([&signal](error_code) {
        signal.emit(asio::cancellation_type::all);
    });

    ioc.run_for(6s);

    BOOST_TEST(broker.received_all_expected());
    BOOST_TEST(handlers_called == expected_handlers_called);
}

BOOST_AUTO_TEST_SUITE_END()

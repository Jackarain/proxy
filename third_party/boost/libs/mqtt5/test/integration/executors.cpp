//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mqtt5/impl/codecs/message_encoders.hpp>

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/bind_immediate_executor.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/mqtt5.hpp>
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "test_common/message_exchange.hpp"
#include "test_common/test_stream.hpp"

using namespace boost::mqtt5;

using strand_type = asio::strand<asio::any_io_executor>;

BOOST_AUTO_TEST_SUITE(executors)

template <typename AsyncRunOp, typename AsyncOp>
void run_test(
    asio::io_context& ioc, strand_type io_ex,
    AsyncRunOp&& bind_async_run, AsyncOp&& bind_async_op
) {
    using test::after;
    using namespace std::chrono_literals;

    constexpr int expected_handlers_called = 8;
    int handlers_called = 0;

    // packets
    auto connect = encoders::encode_connect(
        "", std::nullopt, std::nullopt, 60, false, {}, std::nullopt
    );
    auto connack = encoders::encode_connack(false, reason_codes::success.value(), {});
    auto publish_0 = encoders::encode_publish(
        0, "t_0", "p_0", qos_e::at_most_once, retain_e::no, dup_e::no, {}
    );
    auto publish_1 = encoders::encode_publish(
        1, "t_1", "p_1", qos_e::at_least_once, retain_e::no, dup_e::no, {}
    );
    auto puback = encoders::encode_puback(1, reason_codes::success.value(), {});
    auto publish_2 = encoders::encode_publish(
        2, "t_2", "p_2", qos_e::exactly_once, retain_e::no, dup_e::no, {}
    );
    auto pubrec = encoders::encode_pubrec(2, reason_codes::success.value(), {});
    auto pubrel = encoders::encode_pubrel(2, reason_codes::success.value(), {});
    auto pubcomp = encoders::encode_pubcomp(2, reason_codes::success.value(), {});
    auto subscribe = encoders::encode_subscribe(
        3, std::vector<subscribe_topic> { { "t_0", {} } }, {}
    );
    auto suback = encoders::encode_suback(
        3, std::vector<uint8_t> { reason_codes::granted_qos_2.value() }, {}
    );
    auto unsubscribe = encoders::encode_unsubscribe(
        1, std::vector<std::string> { "t_0" }, {}
    );
    auto unsuback = encoders::encode_unsuback(
        1, std::vector<uint8_t> { reason_codes::success.value() }, {}
    );
    auto disconnect = encoders::encode_disconnect(
        reason_codes::normal_disconnection.value(), {}
    );

    test::msg_exchange broker_side;
    error_code success {};

    broker_side
        .expect(connect)
            .complete_with(success, after(0ms))
            .reply_with(connack, after(0ms))
        .expect(subscribe, publish_0, publish_1, publish_2)
            .complete_with(success, after(0ms))
            .reply_with(puback, pubrec, suback, after(0ms))
        .expect(pubrel)
            .complete_with(success, after(0ms))
            .reply_with(pubcomp, after(0ms))
        .send(publish_0, after(50ms))
        .expect(unsubscribe)
            .complete_with(success, after(0ms))
            .reply_with(unsuback, after(0ms))
        .expect(disconnect)
            .complete_with(success, after(0ms))
        ;

    auto& broker = asio::make_service<test::test_broker>(
        ioc, io_ex, std::move(broker_side)
    );

    using client_type = mqtt_client<test::test_stream>;
    client_type c(io_ex);
    c.brokers("127.0.0.1")
        .async_run(
            bind_async_run(
                [&](strand_type strand, error_code ec) {
                    BOOST_TEST(ec == asio::error::operation_aborted);
                    BOOST_TEST(strand.running_in_this_thread());
                    ++handlers_called;
                }
            )
        );

    c.async_publish<qos_e::at_most_once>(
        "t_0", "p_0", retain_e::no, {},
        bind_async_op(
            [&](strand_type strand, error_code ec) {
                BOOST_TEST(!ec);
                BOOST_TEST(strand.running_in_this_thread());
                ++handlers_called;
            }
        )
    );

    c.async_publish<qos_e::at_least_once>(
        "t_1", "p_1", retain_e::no, {},
        bind_async_op(
            [&](strand_type strand, error_code ec, reason_code rc, auto) {
                BOOST_TEST(!ec);
                BOOST_TEST(!rc);
                BOOST_TEST(strand.running_in_this_thread());
                ++handlers_called;
            }
        )
    );

    c.async_publish<qos_e::exactly_once>(
        "t_2", "p_2", retain_e::no, {},
        bind_async_op(
            [&](strand_type strand, error_code ec, reason_code rc, auto) {
                BOOST_TEST(!ec);
                BOOST_TEST(!rc);
                BOOST_TEST(strand.running_in_this_thread());
                ++handlers_called;
            }
        )
    );

    c.async_subscribe(
        subscribe_topic { "t_0", {} }, {},
        bind_async_op(
            [&](
                strand_type strand,
                error_code ec, std::vector<reason_code> rcs, auto
            ) {
                BOOST_TEST(!ec);
                BOOST_TEST(!rcs[0]);
                BOOST_TEST(strand.running_in_this_thread());
                ++handlers_called;
            }
        )
    );

    c.async_receive(
        bind_async_op(
            [&](
                strand_type strand, error_code ec,
                std::string rec_topic, std::string rec_payload,
                publish_props
            ) {
                BOOST_TEST(!ec);
                BOOST_TEST("t_0" == rec_topic);
                BOOST_TEST("p_0" == rec_payload);
                BOOST_TEST(strand.running_in_this_thread());
                ++handlers_called;

                c.async_unsubscribe(
                    "t_0", {},
                    bind_async_op(
                        [&](
                            strand_type strand,
                            error_code ec, std::vector<reason_code> rcs, auto
                        ) {
                            BOOST_TEST(!ec);
                            BOOST_TEST(!rcs[0]);
                            BOOST_TEST(strand.running_in_this_thread());
                            ++handlers_called;

                            c.async_disconnect(
                                bind_async_op(
                                    [&](strand_type strand, error_code ec) {
                                        BOOST_TEST(!ec);
                                        BOOST_TEST(strand.running_in_this_thread());
                                        ++handlers_called;
                                    }
                                )
                            );
                        }
                    )
                );
            }
        )
    );

    ioc.run_for(500ms);
    BOOST_TEST(handlers_called == expected_handlers_called);
    BOOST_TEST(broker.received_all_expected());
}

BOOST_AUTO_TEST_CASE(different_bound_executors) {
    asio::io_context ioc;
    auto bind_async_op = [&](auto h) {
        auto strand = asio::make_strand(ioc);
        return asio::bind_executor(
            strand,
            asio::prepend(std::move(h), strand)
        );
    };
    run_test(ioc, asio::make_strand(ioc), bind_async_op, bind_async_op);
}

BOOST_AUTO_TEST_CASE(default_executor) {
    asio::io_context ioc;
    auto io_ex = asio::make_strand(ioc);
    auto bind_async_run = [&](auto h) {
        auto strand = asio::make_strand(ioc);
        return asio::bind_executor(
            strand,
            asio::prepend(std::move(h), strand)
        );
    };
    auto bind_async_op = [&](auto h) {
        return asio::prepend(std::move(h), io_ex);
    };
    run_test(ioc, io_ex, bind_async_run, bind_async_op);
}

BOOST_AUTO_TEST_CASE(immediate_executor_async_publish) {
    constexpr int expected_handlers_called = 1;
    int handlers_called = 0;

    asio::io_context ioc;

    using client_type = mqtt_client<asio::ip::tcp::socket>;
    client_type c(ioc.get_executor());

    auto strand = asio::make_strand(ioc);

    auto handler = asio::bind_immediate_executor(
        strand,
        [&handlers_called, &strand](error_code ec) {
            ++handlers_called;
            BOOST_TEST(strand.running_in_this_thread());
            BOOST_TEST(ec);
        }
    );
    c.async_publish<qos_e::at_most_once>(
        "invalid/#", "", retain_e::no, publish_props {}, std::move(handler)
    );

    ioc.run();
    BOOST_TEST(handlers_called == expected_handlers_called);
}

BOOST_AUTO_TEST_CASE(immediate_executor_async_subscribe) {
    constexpr int expected_handlers_called = 1;
    int handlers_called = 0;

    asio::io_context ioc;

    using client_type = mqtt_client<asio::ip::tcp::socket>;
    client_type c(ioc.get_executor());

    auto strand = asio::make_strand(ioc);

    auto handler = asio::bind_immediate_executor(
        strand,
        [&handlers_called, &strand](
            error_code ec, std::vector<reason_code>, suback_props
        ) {
            ++handlers_called;
            BOOST_TEST(strand.running_in_this_thread());
            BOOST_TEST(ec);
        }
    );
    c.async_subscribe(
        { "+topic", subscribe_options {} }, subscribe_props{}, std::move(handler)
    );

    ioc.run();
    BOOST_TEST(handlers_called == expected_handlers_called);
}

BOOST_AUTO_TEST_CASE(immediate_executor_async_unsubscribe) {
    constexpr int expected_handlers_called = 1;
    int handlers_called = 0;

    asio::io_context ioc;

    using client_type = mqtt_client<asio::ip::tcp::socket>;
    client_type c(ioc.get_executor());

    auto strand = asio::make_strand(ioc);

    auto handler = asio::bind_immediate_executor(
        strand,
        [&handlers_called, &strand](
            error_code ec, std::vector<reason_code>, unsuback_props
        ) {
            ++handlers_called;
            BOOST_TEST(strand.running_in_this_thread());
            BOOST_TEST(ec);
        }
    );
    c.async_unsubscribe(
        "some/topic#", unsubscribe_props {}, std::move(handler)
    );

    ioc.run();
    BOOST_TEST(handlers_called == expected_handlers_called);
}


BOOST_AUTO_TEST_SUITE_END()

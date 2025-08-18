//
// Copyright (c) 2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mqtt5/impl/endpoints.hpp>

#include <boost/test/unit_test.hpp>

#include <boost/asio/use_awaitable.hpp>
#ifdef BOOST_ASIO_HAS_CO_AWAIT

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>

#include <chrono>

using namespace boost::mqtt5;

BOOST_AUTO_TEST_SUITE(resolve_op)

struct shared_test_data {
    error_code success {};

    asio::io_context ioc;
    asio::steady_timer timer;
    detail::log_invoke<noop_logger> logger;
    detail::endpoints<noop_logger> ep;

    shared_test_data()
        : timer(ioc.get_executor()), ep(ioc.get_executor(), timer, logger) {}
};

using namespace std::chrono_literals;
constexpr auto use_nothrow_awaitable = asio::as_tuple(asio::use_awaitable);

BOOST_FIXTURE_TEST_CASE(empty_path, shared_test_data) {
    bool finished = false;
    ep.brokers("", 1000);

    asio::co_spawn(ioc, [&]() -> asio::awaitable<void> {
        auto [ec, eps, ap] = co_await ep.async_next_endpoint(use_nothrow_awaitable);
        BOOST_TEST(ec == asio::error::host_not_found);
    }, [&finished](auto eptr) { finished = !eptr; });

    ioc.run_for(1s);
    BOOST_TEST(finished);
}

BOOST_FIXTURE_TEST_CASE(single_host, shared_test_data) {
    bool finished = false;
    ep.brokers("127.0.0.1", 1000);

    asio::co_spawn(ioc, [&]() -> asio::awaitable<void> {
        auto [ec, eps, ap] = co_await ep.async_next_endpoint(use_nothrow_awaitable);
        BOOST_TEST(ec == success);
        BOOST_TEST(!eps.empty());
        BOOST_TEST(ap.host == "127.0.0.1");
        BOOST_TEST(ap.port == "1000");
        BOOST_TEST(ap.path.empty());

        std::tie(ec, eps, ap) = co_await ep.async_next_endpoint(use_nothrow_awaitable);
        BOOST_TEST(ec == asio::error::try_again);
    }, [&finished](auto eptr) { finished = !eptr; });

    ioc.run_for(1s);
    BOOST_TEST(finished);
}

BOOST_FIXTURE_TEST_CASE(multiple_hosts, shared_test_data) {
    bool finished = false;
    // "example.invalid" will not be resolved
    ep.brokers("127.0.0.1:1001,127.0.0.1/path1, example.invalid,   localhost:1002/path1/path-2/path.3_", 1000);

    asio::co_spawn(ioc, [&]() -> asio::awaitable<void> {
        for (size_t i = 0; i < 3; ++i) {
            auto [ec, eps, ap] = co_await ep.async_next_endpoint(use_nothrow_awaitable);
            BOOST_TEST(ec == success);
            BOOST_TEST(!eps.empty());
            BOOST_TEST(ap.host == "127.0.0.1");
            BOOST_TEST(ap.port == "1001");
            BOOST_TEST(ap.path.empty());

            std::tie(ec, eps, ap) = co_await ep.async_next_endpoint(use_nothrow_awaitable);
            BOOST_TEST(ec == success);
            BOOST_TEST(!eps.empty());
            BOOST_TEST(ap.host == "127.0.0.1");
            BOOST_TEST(ap.port == "1000");
            BOOST_TEST(ap.path == "/path1");

            std::tie(ec, eps, ap) = co_await ep.async_next_endpoint(use_nothrow_awaitable);
            BOOST_TEST(ec == success);
            BOOST_TEST(!eps.empty());
            BOOST_TEST(ap.host == "localhost");
            BOOST_TEST(ap.port == "1002");
            BOOST_TEST(ap.path == "/path1/path-2/path.3_");

            std::tie(ec, eps, ap) = co_await ep.async_next_endpoint(use_nothrow_awaitable);
            BOOST_TEST(ec == asio::error::try_again);
        }
    }, [&finished](auto eptr) { finished = !eptr; });

    ioc.run_for(1s);
    BOOST_TEST(finished);
}

BOOST_FIXTURE_TEST_CASE(parse_failure, shared_test_data) {
    bool finished = false;
    ep.brokers("127.0.0.1,127.0.0.1::1883,127.0.0.1", 1000);

    asio::co_spawn(ioc, [&]() -> asio::awaitable<void> {
        auto [ec, eps, ap] = co_await ep.async_next_endpoint(use_nothrow_awaitable);
        BOOST_TEST(ec == success);
        BOOST_TEST(!eps.empty());
        BOOST_TEST(ap.host == "127.0.0.1");
        BOOST_TEST(ap.port == "1000");
        BOOST_TEST(ap.path.empty());

        std::tie(ec, eps, ap) = co_await ep.async_next_endpoint(use_nothrow_awaitable);
        BOOST_TEST(ec == asio::error::try_again);
    }, [&finished](auto eptr) { finished = !eptr; });

    ioc.run_for(1s);
    BOOST_TEST(finished);
}

BOOST_AUTO_TEST_SUITE_END();

#endif // BOOST_ASIO_HAS_CO_AWAIT

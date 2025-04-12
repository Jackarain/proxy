//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mqtt5/types.hpp>

#include <boost/mqtt5/detail/async_mutex.hpp>

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <cstdint>
#include <string>

using namespace boost::mqtt5;
using error_code = boost::system::error_code;
using async_mutex = detail::async_mutex;

BOOST_AUTO_TEST_SUITE(async_mutex_unit/*, *boost::unit_test::disabled()*/)

BOOST_AUTO_TEST_CASE(lock_mutex) {
    constexpr int expected_handlers_called = 1;
    int handlers_called = 0;

    asio::thread_pool tp(1);
    async_mutex mutex(tp.executor());

    mutex.lock([&mutex, &handlers_called](error_code ec) {
        ++handlers_called;
        BOOST_TEST(!ec);
        BOOST_TEST(mutex.is_locked());
        mutex.unlock();
        BOOST_TEST(!mutex.is_locked());
    });

    tp.wait();
    BOOST_TEST(handlers_called == expected_handlers_called);
}

BOOST_AUTO_TEST_CASE(get_executor) {
    asio::thread_pool tp(1);
    auto ex = tp.get_executor();
    async_mutex mutex(ex);
    BOOST_CHECK(mutex.get_executor() == ex);
}

BOOST_AUTO_TEST_CASE(bind_executor) {
    constexpr int expected_handlers_called = 2;
    int handlers_called = 0;

    asio::thread_pool tp(1);

    async_mutex mutex(tp.get_executor());
    auto s1 = asio::make_strand(tp.get_executor());
    auto s2 = asio::make_strand(tp.get_executor());

    mutex.lock(
        asio::bind_executor(
            s1,
            [&](error_code ec) mutable {
                ++handlers_called;
                BOOST_TEST(!ec);
                BOOST_TEST(s1.running_in_this_thread());
                BOOST_TEST(!s2.running_in_this_thread());
                mutex.unlock();
            }
        )
    );

    mutex.lock(
        asio::bind_executor(
            s2,
            [&](error_code ec) mutable {
                ++handlers_called;
                BOOST_TEST(!ec);
                BOOST_TEST(!s1.running_in_this_thread());
                BOOST_TEST(s2.running_in_this_thread());
                mutex.unlock();
            }
        )
    );

    tp.wait();
    BOOST_TEST(handlers_called == expected_handlers_called);
    BOOST_TEST(!mutex.is_locked());
}

BOOST_AUTO_TEST_CASE(per_op_cancellation) {
    constexpr int expected_handlers_called = 2;
    int handlers_called = 0;

    asio::io_context ioc;
    asio::cancellation_signal cs;

    async_mutex mutex(asio::make_strand(ioc.get_executor()));

    // mutex must be locked in order to cancel a pending operation
    mutex.lock(
        [&mutex, &handlers_called](error_code ec) {
            ++handlers_called;
            BOOST_TEST(!ec);
            mutex.unlock();
        }
    );

    mutex.lock(
        asio::bind_cancellation_slot(
            cs.slot(),
            [&handlers_called](error_code ec) {
                ++handlers_called;
                BOOST_TEST(ec == asio::error::operation_aborted);
            }
        )
    );
    cs.emit(asio::cancellation_type_t::terminal);
    cs.slot().clear();

    ioc.run();
    BOOST_TEST(handlers_called == expected_handlers_called);
    BOOST_TEST(!mutex.is_locked());
}

BOOST_AUTO_TEST_CASE(cancel_ops_by_destructor) {
    constexpr int expected_handlers_called = 2;
    int handlers_called = 0;

    asio::io_context ioc;

    {
        async_mutex mutex(ioc.get_executor());

        auto op = [&handlers_called](error_code ec) {
            handlers_called++;
            BOOST_TEST(!ec);
        };

        auto cancelled_op = [&handlers_called](error_code ec) {
            handlers_called++;
            BOOST_TEST(ec == asio::error::operation_aborted);
        };

        mutex.lock(std::move(op)); // will be immediately posted with ec = success
        mutex.lock(std::move(cancelled_op));
    }

    ioc.run();
    BOOST_TEST(handlers_called == expected_handlers_called);
}

BOOST_AUTO_TEST_CASE(cancel_ops) {
    constexpr int expected_handlers_called = 5;
    int handlers_called = 0;

    asio::io_context ioc;
    async_mutex mutex(ioc.get_executor());

    auto op = [&mutex, &handlers_called](error_code ec) {
        handlers_called++;
        BOOST_TEST(!ec);
        mutex.unlock();
    };

    auto cancelled_op = [&handlers_called](error_code ec) {
        handlers_called++;
        BOOST_TEST(ec == asio::error::operation_aborted);
    };

    mutex.lock(std::move(op));

    // pending operations that will be cancelled
    for (int i = 0; i < expected_handlers_called - 1; ++i)
        mutex.lock(cancelled_op);

    mutex.cancel();
    ioc.run();
    BOOST_TEST(handlers_called == expected_handlers_called);
    BOOST_TEST(!mutex.is_locked());
}

BOOST_AUTO_TEST_SUITE_END()

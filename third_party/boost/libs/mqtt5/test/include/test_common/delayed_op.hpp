//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_TEST_DELAYED_OP_HPP
#define BOOST_MQTT5_TEST_DELAYED_OP_HPP

#include <boost/asio/append.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/prepend.hpp>
#include <boost/type_traits/remove_cv_ref.hpp>

#include <chrono>

#include "test_timer.hpp"

namespace boost::mqtt5::test {

namespace asio = boost::asio;

using error_code = boost::system::error_code;
using time_stamp = clock::time_point;
using duration = time_stamp::duration;
using rep = duration::rep;

template <typename... BoundArgs>
struct delayed_op {
    duration delay;
    std::tuple<BoundArgs...> args;

    delayed_op(duration delay, BoundArgs... args)
        : delay(delay), args(std::move(args)...)
    {}
};

template <typename CompletionToken, typename Executor, typename... BoundArgs>
decltype(auto) async_delay(
    const Executor& ex,
    delayed_op<BoundArgs...> op,
    CompletionToken&& token
) {
    using Signature = void (error_code, boost::remove_cv_ref_t<BoundArgs>...);
    auto initiation = [](auto handler, const Executor& ex, auto op) {
        auto timer = std::make_unique<asio::basic_waitable_timer<clock>>(ex);
        timer->expires_after(op.delay);
        auto bound_handler = std::apply(
            [h = std::move(handler)](auto&&... args) mutable {
                return asio::append(std::move(h), std::move(args)...);
            },
            std::move(op.args)
        );
        timer->async_wait(
            asio::consign(
                [bh = std::move(bound_handler)](error_code ec) mutable {
                    asio::dispatch(asio::prepend(std::move(bh), ec));
                }, std::move(timer)
            )
        );
    };

    return asio::async_initiate<CompletionToken, Signature>(
        std::move(initiation), token, ex, std::move(op)
    );
}

} // end namespace boost::mqtt5::test

#endif // BOOST_MQTT5_TEST_DELAYED_OP_HPP

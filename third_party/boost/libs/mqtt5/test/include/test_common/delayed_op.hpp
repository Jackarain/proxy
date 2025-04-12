//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_TEST_DELAYED_OP_HPP
#define BOOST_MQTT5_TEST_DELAYED_OP_HPP

#include <boost/asio/append.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/prepend.hpp>
#include <boost/asio/recycling_allocator.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/type_traits/remove_cv_ref.hpp>

#include <chrono>

namespace boost::mqtt5::test {

namespace asio = boost::asio;

using error_code = boost::system::error_code;
using time_stamp = std::chrono::time_point<std::chrono::steady_clock>;
using duration = time_stamp::duration;

template <typename ...BoundArgs>
class delayed_op {
    struct on_timer {};

    std::unique_ptr<asio::steady_timer> _timer;
    time_stamp::duration _delay;
    asio::cancellation_slot _cancel_slot;

    std::tuple<BoundArgs...> _args;

public:
    template <typename Executor, typename ...Args>
    delayed_op(
        const Executor& ex, time_stamp::duration delay, Args&& ...args
    ) :
        _timer(new asio::steady_timer(ex)), _delay(delay),
        _args(std::move(args)...)
    {}

    delayed_op(delayed_op&&) = default;
    delayed_op(const delayed_op&) = delete;

    delayed_op& operator=(delayed_op&&) = default;
    delayed_op& operator=(const delayed_op&) = delete;

    using allocator_type = asio::recycling_allocator<void>;
    allocator_type get_allocator() const noexcept {
        return allocator_type {};
    }

    using cancellation_slot_type = asio::cancellation_slot;
    asio::cancellation_slot get_cancellation_slot() const noexcept {
        return _cancel_slot;
    }

    using executor_type = asio::steady_timer::executor_type;
    executor_type get_executor() const noexcept {
        return _timer->get_executor();
    }

    template <typename CompletionHandler>
    void perform(CompletionHandler&& handler) {
        _cancel_slot = asio::get_associated_cancellation_slot(handler);

        _timer->expires_after(_delay);
        _timer->async_wait(
            asio::prepend(std::move(*this), on_timer {}, std::move(handler))
        );
    }

    template <typename CompletionHandler>
    void operator()(on_timer, CompletionHandler&& h, error_code ec) {
        // The timer places a handler into the cancellation slot
        // and does not clear it. Therefore, we need to clear it explicitly
        // to properly remove the corresponding cancellation signal
        // in the test_broker.
        get_cancellation_slot().clear();

        auto bh = std::apply(
            [h = std::move(h)](auto&&... args) mutable {
                return asio::append(std::move(h), std::move(args)...);
            },
            _args
        );

        asio::dispatch(asio::prepend(std::move(bh), ec));
    }
};

template <typename CompletionToken, typename ...BoundArgs>
decltype(auto) async_delay(
    asio::cancellation_slot cancel_slot,
    delayed_op<BoundArgs...>&& op,
    CompletionToken&& token
) {
    using Signature = void (error_code, boost::remove_cv_ref_t<BoundArgs>...);

    auto initiation = [](
        auto handler, asio::cancellation_slot cancel_slot,
        delayed_op<BoundArgs...> op
    ) {
        op.perform(
            asio::bind_cancellation_slot(cancel_slot, std::move(handler))
        );
    };

    return asio::async_initiate<CompletionToken, Signature>(
        std::move(initiation), token, cancel_slot, std::move(op)
    );
}


} // end namespace boost::mqtt5::test

#endif // BOOST_MQTT5_TEST_DELAYED_OP_HPP

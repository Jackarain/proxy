//
// Copyright (c) 2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_TEST_TIMER_HPP
#define BOOST_MQTT5_TEST_TIMER_HPP

#include <vector>
#include <utility>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/prepend.hpp>

#include <boost/system/error_code.hpp>

namespace boost::mqtt5::test {

namespace asio = boost::asio;

class clock {
public:
    using duration = std::chrono::duration<int64_t, std::milli>;
    using time_point = std::chrono::time_point<clock, duration>;
    using rep = duration::rep;
    using period = duration::period;

    static constexpr bool is_steady = false;

    static time_point now() noexcept {
        std::terminate();
    }
};

template <typename Clock>
class timer_service : public boost::asio::execution_context::service {
public:
    static inline boost::asio::execution_context::id id;

private:
    using base = boost::asio::execution_context::service;
    using error_code = boost::system::error_code;
    using test_timer = boost::asio::basic_waitable_timer<Clock>;

    std::vector<test_timer*> _timers;
    typename Clock::time_point _simulation_time;

public:
    explicit timer_service(boost::asio::execution_context& context)
        : base(context) {}

    void add_timer(test_timer* timer) {
        _timers.push_back(timer);
    }

    void remove_timer(test_timer* timer) {
        auto it = std::remove(_timers.begin(), _timers.end(), timer);
        _timers.erase(it);
    }

    void advance() {
        if (_timers.empty()) return;
        auto it = std::min_element(
            _timers.begin(), _timers.end(),
            [](const test_timer* fst, const test_timer* snd) {
                return fst->expiry() < snd->expiry();
            }
        );
        _simulation_time = (*it)->expiry();
        for (auto it = _timers.begin(); it != _timers.end();) {
            auto* timer = *it;
            if (timer->expiry() <= now()) {
                timer->complete_post(error_code {});
                it = _timers.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    auto now() const { return _simulation_time; }

private:
    void shutdown() noexcept override {
        for (auto* timer : _timers)
            timer->complete_post(asio::error::operation_aborted);
        _timers.clear();
    }
};

using test_timer = asio::basic_waitable_timer<clock>;

} // namespace boost::mqtt5::test


namespace boost::asio {

template <>
class basic_waitable_timer<boost::mqtt5::test::clock> {
public:
    using executor_type = asio::any_io_executor;
    using clock_type = boost::mqtt5::test::clock;
    using duration = clock_type::duration;
    using time_point = clock_type::time_point;

private:
    using error_code = boost::system::error_code;
    using service = boost::mqtt5::test::timer_service<clock_type>;
    using signature = void (error_code);

    friend class boost::mqtt5::test::timer_service<clock_type>;

    executor_type _ex;
    time_point _expires_at;
    asio::any_completion_handler<signature> _handler;
    asio::prefer_result_t<
        executor_type, execution::outstanding_work_t::tracked_t> _handler_ex;

public:
    explicit basic_waitable_timer(executor_type ex) : _ex(std::move(ex)) {}
    basic_waitable_timer(executor_type ex, time_point expiry_time)
        : _ex(std::move(ex)) { expires_at(expiry_time); }
    basic_waitable_timer(executor_type ex, duration dur)
        : _ex(std::move(ex)) { expires_after(dur); }

    ~basic_waitable_timer() {
        if (_handler) {
            get_service().remove_timer(this);
            complete_post(asio::error::operation_aborted);
        }
    }

    executor_type get_executor() const noexcept { return _ex; }

    size_t expires_at(time_point expiry_time) {
        _expires_at = expiry_time;
        return 0;
    }

    size_t expires_after(duration dur) {
        auto now = get_service().now();
        if ((time_point::max)() - now < dur)
            _expires_at = (time_point::max)();
        else
            _expires_at = now + dur;
        return 0;
    }

    time_point expiry() const { return _expires_at; }
    size_t cancel() {
        if (_handler) {
            get_service().remove_timer(this);
            complete_post(asio::error::operation_aborted);
            return 1;
        }
        return 0;
    }

    template <typename CompletionToken>
    decltype(auto) async_wait(CompletionToken&& token) {
        auto initiation = [this](auto handler) {
            if (_expires_at <= get_service().now())
                return asio::post(
                    get_executor(),
                    asio::prepend(std::move(handler), error_code {})
                );

            _handler = std::move(handler);
            _handler_ex = asio::prefer(_ex, asio::execution::outstanding_work.tracked);

            auto slot = asio::get_associated_cancellation_slot(_handler);
            if (slot.is_connected())
                slot.assign([this](asio::cancellation_type_t type) {
                    if (type != asio::cancellation_type_t::none)
                        cancel();
                });

            get_service().add_timer(this);
        };

        return asio::async_initiate<CompletionToken, signature>(
            initiation, token
        );
    }

private:
    service& get_service() const {
        return use_service<service>(_ex.context());
    }

    void complete_post(error_code ec) {
        asio::get_associated_cancellation_slot(_handler).clear();
        asio::post(get_executor(), asio::prepend(std::move(_handler), ec));
        _handler_ex = {};
    }
};

} // namespace boost::asio

#endif // !BOOST_MQTT5_TEST_TIMER_HPP

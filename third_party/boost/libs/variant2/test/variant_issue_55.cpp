// Copyright 2025 Peter Dimov
// Copyright 2025 Vinnie Falco
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

// GCC 12+ -Wmaybe-uninitialized false positive tests
// https://github.com/boostorg/variant2/issues/55
//
// GCC 12+'s improved dataflow analysis sees code paths for all alternatives
// in mp_with_index and warns that members may be uninitialized, even though
// the variant's discriminator guarantees only initialized alternatives are
// accessed.

#include <boost/system/result.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <exception>
#include <string>

// Check for C++17 std::optional support
#if BOOST_CXX_VERSION >= 201703L
# include <optional>
# define BOOST_VARIANT2_TEST_HAS_OPTIONAL 1
#endif

// Check for C++20 coroutine support
#if defined(__cpp_impl_coroutine) && __cpp_impl_coroutine >= 201902L
# include <coroutine>
# define BOOST_VARIANT2_TEST_HAS_CORO 1
#endif

using result_void = boost::system::result<void, std::exception_ptr>;
using result_string = boost::system::result<std::string, std::exception_ptr>;

void testGccUninitialized()
{
    // Test 1: Simple copy construction
    {
        result_void r1;
        result_void r2(r1);
        (void)r2;
    }

    // Test 2: Copy assignment
    {
        result_void r1;
        result_void r2;
        r2 = r1;
        (void)r2;
    }

#ifdef BOOST_VARIANT2_TEST_HAS_OPTIONAL
    // Test 3: std::optional assignment (matches spawn pattern)
    {
        std::optional<result_void> opt;
        opt = result_void{};
        (void)opt;
    }
#endif

    // Test 4: Pass to function via copy
    {
        auto fn = [](result_void r) { (void)r; };
        fn(result_void{});
    }

#ifdef BOOST_VARIANT2_TEST_HAS_OPTIONAL
    // Test 5: Lambda capture + optional (closest to spawn)
    {
        auto fn = [](result_void r) {
            std::optional<result_void> opt;
            opt = r;
            return opt.has_value();
        };
        (void)fn(result_void{});
    }
#endif

    // Test 6: Non-void result with string (triggers string warning)
    {
        result_string r1;
        result_string r2(r1);
        (void)r2;
    }

    // Test 7: Assign exception to result holding value
    {
        result_string r1{"hello"};
        r1 = std::make_exception_ptr(std::runtime_error("test"));
        (void)r1;
    }

#ifdef BOOST_VARIANT2_TEST_HAS_OPTIONAL
    // Test 8: Optional with string result
    {
        std::optional<result_string> opt;
        opt = result_string{};
        (void)opt;
    }
#endif

#ifdef BOOST_VARIANT2_TEST_HAS_CORO
    // Minimal fire-and-forget coroutine for testing
    struct fire_and_forget
    {
        struct promise_type
        {
            fire_and_forget get_return_object() { return {}; }
            std::suspend_never initial_suspend() noexcept { return {}; }
            std::suspend_never final_suspend() noexcept { return {}; }
            void return_void() {}
            void unhandled_exception() { std::terminate(); }
        };
    };

    // Test 9: Coroutine returning result (mimics spawn)
    {
        auto coro = []() -> fire_and_forget {
            result_void r{};
            (void)r;
            co_return;
        };
        coro();
    }

    // Test 10: Coroutine with handler call (closest to actual spawn)
    {
        std::optional<result_void> received;
        auto handler = [&](result_void r) {
            received = r;
        };
        auto coro = [&]() -> fire_and_forget {
            handler(result_void{});
            co_return;
        };
        coro();
        (void)received;
    }

    // Test 11: Coroutine with try/catch like spawn
    {
        std::optional<result_void> received;
        auto handler = [&](result_void r) {
            received = r;
        };
        auto coro = [&]() -> fire_and_forget {
            try
            {
                handler(result_void{});
            }
            catch (...)
            {
                handler(result_void{std::current_exception()});
            }
            co_return;
        };
        coro();
        (void)received;
    }

    // Test 12: Coroutine with string result
    {
        std::optional<result_string> received;
        auto handler = [&](result_string r) {
            received = r;
        };
        auto coro = [&]() -> fire_and_forget {
            try
            {
                handler(result_string{"test"});
            }
            catch (...)
            {
                handler(result_string{std::current_exception()});
            }
            co_return;
        };
        coro();
        (void)received;
    }
#endif
}

int main()
{
    boost::core::lwt_init();

    testGccUninitialized();

    return boost::report_errors();
}

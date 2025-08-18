//  Copyright (c) 2020-2025 Andrey Semashev
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_ATOMIC_TEST_IPC_WAIT_TEST_HELPERS_HPP_INCLUDED_
#define BOOST_ATOMIC_TEST_IPC_WAIT_TEST_HELPERS_HPP_INCLUDED_

#include <boost/atomic/ipc_atomic_flag.hpp>
#include <boost/atomic/wait_result.hpp>

#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
#include <memory>
#include <utility>
#include <iostream>
#include <type_traits>
#include <boost/config.hpp>
#include <boost/current_function.hpp>
#include <boost/atomic/capabilities.hpp>
#include <boost/atomic/ipc_atomic_flag.hpp>
#include "atomic_wrapper.hpp"
#include "lightweight_test_stream.hpp"
#include "test_thread.hpp"
#include "test_barrier.hpp"

//! Since some of the tests below are allowed to fail, we retry up to this many times to pass the test
constexpr unsigned int test_retry_count = 10u;

//! The test verifies that the wait operation returns immediately if the passed value does not match the atomic value
template< template< typename > class Wrapper, typename T >
inline void test_wait_value_mismatch(T value1, T value2)
{
    Wrapper< T > m_wrapper(value1);

    {
        T received_value = m_wrapper.a.wait(value2);
        BOOST_TEST(received_value == value1);
    }
    {
        boost::atomics::wait_result< T > result = m_wrapper.a.wait_until(value2, std::chrono::steady_clock::now());
        BOOST_TEST(result.value == value1);
        BOOST_TEST(!result.timeout);
    }
    {
        boost::atomics::wait_result< T > result = m_wrapper.a.wait_until(value2, std::chrono::steady_clock::now() + std::chrono::milliseconds(200));
        BOOST_TEST(result.value == value1);
        BOOST_TEST(!result.timeout);
    }
    {
        boost::atomics::wait_result< T > result = m_wrapper.a.wait_for(value2, std::chrono::milliseconds::zero());
        BOOST_TEST(result.value == value1);
        BOOST_TEST(!result.timeout);
    }
    {
        boost::atomics::wait_result< T > result = m_wrapper.a.wait_for(value2, std::chrono::milliseconds(200));
        BOOST_TEST(result.value == value1);
        BOOST_TEST(!result.timeout);
    }
}

/*!
 * The test verifies that notify_one releases one blocked thread and that the released thread receives the modified atomic value.
 *
 * Technically, this test is allowed to fail since wait() is allowed to return spuriously. However, normally this should not happen.
 */
template< template< typename > class Wrapper, typename T >
class notify_one_test
{
private:
    struct thread_state
    {
        T m_received_value;
        std::chrono::steady_clock::time_point m_wakeup_time;

        explicit thread_state(T value) : m_received_value(value)
        {
        }
    };

private:
    Wrapper< T > m_wrapper;

    char m_padding[1024];

    T m_value1, m_value2, m_value3;

    test_barrier m_barrier;

    thread_state m_thread1_state;
    thread_state m_thread2_state;

public:
    explicit notify_one_test(T value1, T value2, T value3) :
        m_wrapper(value1),
        m_value1(value1),
        m_value2(value2),
        m_value3(value3),
        m_barrier(3),
        m_thread1_state(value1),
        m_thread2_state(value1)
    {
    }

    bool run()
    {
        test_thread thread1([this]() { this->thread_func(&this->m_thread1_state); });
        test_thread thread2([this]() { this->thread_func(&this->m_thread2_state); });

        m_barrier.arrive_and_wait();

        std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();

        std::this_thread::sleep_until(start_time + std::chrono::milliseconds(200));

        m_wrapper.a.store(m_value2);
        m_wrapper.a.notify_one();

        std::this_thread::sleep_until(start_time + std::chrono::milliseconds(500));

        m_wrapper.a.store(m_value3);
        m_wrapper.a.notify_one();

        if (!thread1.try_join_for(std::chrono::seconds(5)))
        {
            BOOST_ERROR("Thread 1 failed to join");
            std::abort();
        }
        if (!thread2.try_join_for(std::chrono::seconds(5)))
        {
            BOOST_ERROR("Thread 2 failed to join");
            std::abort();
        }

        thread_state* first_state = &m_thread1_state;
        thread_state* second_state = &m_thread2_state;
        if (second_state->m_wakeup_time < first_state->m_wakeup_time)
            std::swap(first_state, second_state);

        if (m_wrapper.a.has_native_wait_notify())
        {
            if ((first_state->m_wakeup_time - start_time) < std::chrono::milliseconds(200))
            {
                std::cout << BOOST_CURRENT_FUNCTION << ": first thread woke up too soon: "
                    << std::chrono::duration_cast< std::chrono::milliseconds >(first_state->m_wakeup_time - start_time).count() << " ms" << std::endl;
                return false;
            }

            if ((first_state->m_wakeup_time - start_time) >= std::chrono::milliseconds(500))
            {
                std::cout << BOOST_CURRENT_FUNCTION << ": first thread woke up too late: "
                    << std::chrono::duration_cast< std::chrono::milliseconds >(first_state->m_wakeup_time - start_time).count() << " ms" << std::endl;
                return false;
            }

            if ((second_state->m_wakeup_time - start_time) < std::chrono::milliseconds(500))
            {
                std::cout << BOOST_CURRENT_FUNCTION << ": second thread woke up too soon: "
                    << std::chrono::duration_cast< std::chrono::milliseconds >(second_state->m_wakeup_time - start_time).count() << " ms" << std::endl;
                return false;
            }

            // Sometimes, even with the time check above, the second thread receives value2. This mostly happens in VMs.
            if (second_state->m_received_value == m_value2)
            {
                std::cout << BOOST_CURRENT_FUNCTION << ": second thread received value2 after waiting for "
                    << std::chrono::duration_cast< std::chrono::milliseconds >(second_state->m_wakeup_time - start_time).count() << " ms" << std::endl;
                return false;
            }

            BOOST_TEST_EQ(first_state->m_received_value, m_value2);
            BOOST_TEST_EQ(second_state->m_received_value, m_value3);
        }
        else
        {
            // With the emulated wait/notify the threads are most likely to return prior to notify
            BOOST_TEST(first_state->m_received_value == m_value2 || first_state->m_received_value == m_value3);
            BOOST_TEST(second_state->m_received_value == m_value2 || second_state->m_received_value == m_value3);
        }

        return true;
    }

private:
    void thread_func(thread_state* state)
    {
        m_barrier.arrive_and_wait();

        state->m_received_value = m_wrapper.a.wait(m_value1);
        state->m_wakeup_time = std::chrono::steady_clock::now();
    }
};

template< template< typename > class Wrapper, typename T >
inline void test_notify_one(T value1, T value2, T value3)
{
    for (unsigned int i = 0u; i < test_retry_count; ++i)
    {
        // Avoid creating IPC atomics on the stack as this breaks on Darwin
        std::unique_ptr< notify_one_test< Wrapper, T > > test(new notify_one_test< Wrapper, T >(value1, value2, value3));
        if (test->run())
            return;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    BOOST_ERROR("notify_one_test could not complete because of the timing issues");
}

/*!
 * The test verifies that notify_all releases all blocked threads and that the released threads receive the modified atomic value.
 *
 * Technically, this test is allowed to fail since wait() is allowed to return spuriously. However, normally this should not happen.
 */
template< template< typename > class Wrapper, typename T >
class notify_all_test
{
private:
    struct thread_state
    {
        T m_received_value;
        std::chrono::steady_clock::time_point m_wakeup_time;

        explicit thread_state(T value) : m_received_value(value)
        {
        }
    };

private:
    Wrapper< T > m_wrapper;

    char m_padding[1024];

    T m_value1, m_value2;

    test_barrier m_barrier;

    thread_state m_thread1_state;
    thread_state m_thread2_state;

public:
    explicit notify_all_test(T value1, T value2) :
        m_wrapper(value1),
        m_value1(value1),
        m_value2(value2),
        m_barrier(3),
        m_thread1_state(value1),
        m_thread2_state(value1)
    {
    }

    bool run()
    {
        test_thread thread1([this]() { this->thread_func(&this->m_thread1_state); });
        test_thread thread2([this]() { this->thread_func(&this->m_thread2_state); });

        m_barrier.arrive_and_wait();

        std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();

        std::this_thread::sleep_until(start_time + std::chrono::milliseconds(200));

        m_wrapper.a.store(m_value2);
        m_wrapper.a.notify_all();

        if (!thread1.try_join_for(std::chrono::seconds(5)))
        {
            BOOST_ERROR("Thread 1 failed to join");
            std::abort();
        }
        if (!thread2.try_join_for(std::chrono::seconds(5)))
        {
            BOOST_ERROR("Thread 2 failed to join");
            std::abort();
        }

        if (m_wrapper.a.has_native_wait_notify())
        {
            if ((m_thread1_state.m_wakeup_time - start_time) < std::chrono::milliseconds(200))
            {
                std::cout << BOOST_CURRENT_FUNCTION << ": first thread woke up too soon: "
                    << std::chrono::duration_cast< std::chrono::milliseconds >(m_thread1_state.m_wakeup_time - start_time).count() << " ms" << std::endl;
                return false;
            }

            if ((m_thread2_state.m_wakeup_time - start_time) < std::chrono::milliseconds(200))
            {
                std::cout << BOOST_CURRENT_FUNCTION << ": second thread woke up too soon: "
                    << std::chrono::duration_cast< std::chrono::milliseconds >(m_thread2_state.m_wakeup_time - start_time).count() << " ms" << std::endl;
                return false;
            }
        }

        BOOST_TEST_EQ(m_thread1_state.m_received_value, m_value2);
        BOOST_TEST_EQ(m_thread2_state.m_received_value, m_value2);

        return true;
    }

private:
    void thread_func(thread_state* state)
    {
        m_barrier.arrive_and_wait();

        state->m_received_value = m_wrapper.a.wait(m_value1);
        state->m_wakeup_time = std::chrono::steady_clock::now();
    }
};

template< template< typename > class Wrapper, typename T >
inline void test_notify_all(T value1, T value2)
{
    for (unsigned int i = 0u; i < test_retry_count; ++i)
    {
        // Avoid creating IPC atomics on the stack as this breaks on Darwin
        std::unique_ptr< notify_all_test< Wrapper, T > > test(new notify_all_test< Wrapper, T >(value1, value2));
        if (test->run())
            return;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    BOOST_ERROR("notify_all_test could not complete because blocked thread wake up too soon");
}

/*!
 * The test verifies that absolute timeout expiry is correctly registered.
 */
template< template< typename > class Wrapper, typename T, typename Clock >
class abs_timeout_test
{
private:
    Wrapper< T > m_wrapper;
    T m_value1;

public:
    explicit abs_timeout_test(T value1) :
        m_wrapper(value1),
        m_value1(value1)
    {
    }

    bool run()
    {
        typename Clock::time_point start_time = Clock::now();

        boost::atomics::wait_result< T > result = m_wrapper.a.wait_until(m_value1, start_time + std::chrono::milliseconds(200));

        typename Clock::time_point wakeup_time = Clock::now();

        if ((wakeup_time - start_time) < std::chrono::milliseconds(200))
        {
            std::cout << BOOST_CURRENT_FUNCTION << ": thread woke up too soon: "
                << std::chrono::duration_cast< std::chrono::milliseconds >(wakeup_time - start_time).count() << " ms" << std::endl;
            return false;
        }

        BOOST_TEST_EQ(result.value, m_value1);
        BOOST_TEST(result.timeout);

        return true;
    }
};

template< template< typename > class Wrapper, typename Clock, typename T >
inline void test_abs_timeout(T value1)
{
    for (unsigned int i = 0u; i < test_retry_count; ++i)
    {
        // Avoid creating IPC atomics on the stack as this breaks on Darwin
        std::unique_ptr< abs_timeout_test< Wrapper, T, Clock > > test(new abs_timeout_test< Wrapper, T, Clock >(value1));
        if (test->run())
            return;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    BOOST_ERROR("abs_timeout_test could not complete because blocked thread wake up too soon");
}

/*!
 * The test verifies that relative timeout expiry is correctly registered.
 */
template< template< typename > class Wrapper, typename T >
class rel_timeout_test
{
private:
    Wrapper< T > m_wrapper;
    T m_value1;

public:
    explicit rel_timeout_test(T value1) :
        m_wrapper(value1),
        m_value1(value1)
    {
    }

    bool run()
    {
        std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();

        boost::atomics::wait_result< T > result = m_wrapper.a.wait_for(m_value1, std::chrono::milliseconds(200));

        std::chrono::steady_clock::time_point wakeup_time = std::chrono::steady_clock::now();

        if ((wakeup_time - start_time) < std::chrono::milliseconds(200))
        {
            std::cout << BOOST_CURRENT_FUNCTION << ": thread woke up too soon: "
                << std::chrono::duration_cast< std::chrono::milliseconds >(wakeup_time - start_time).count() << " ms" << std::endl;
            return false;
        }

        BOOST_TEST_EQ(result.value, m_value1);
        BOOST_TEST(result.timeout);

        return true;
    }
};

template< template< typename > class Wrapper, typename T >
inline void test_rel_timeout(T value1)
{
    for (unsigned int i = 0u; i < test_retry_count; ++i)
    {
        // Avoid creating IPC atomics on the stack as this breaks on Darwin
        std::unique_ptr< rel_timeout_test< Wrapper, T > > test(new rel_timeout_test< Wrapper, T >(value1));
        if (test->run())
            return;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    BOOST_ERROR("rel_timeout_test could not complete because blocked thread wake up too soon");
}

/*!
 * The test verifies that notifying interrupts a waiting operation with an absolute timeout.
 */
template< template< typename > class Wrapper, typename T, typename Clock >
class notify_abs_timeout_test
{
private:
    struct thread_state
    {
        boost::atomics::wait_result< T > m_received_result;
        typename Clock::time_point m_wakeup_time;

        explicit thread_state(T value)
        {
            m_received_result.value = value;
            m_received_result.timeout = true;
        }
    };

private:
    Wrapper< T > m_wrapper;

    char m_padding[1024];

    T m_value1, m_value2;

    test_barrier m_barrier;

    thread_state m_thread1_state;

public:
    explicit notify_abs_timeout_test(T value1, T value2) :
        m_wrapper(value1),
        m_value1(value1),
        m_value2(value2),
        m_barrier(2),
        m_thread1_state(value1)
    {
    }

    bool run()
    {
        test_thread thread1([this]() { this->thread_func(&this->m_thread1_state); });

        m_barrier.arrive_and_wait();

        typename Clock::time_point start_time = Clock::now();

        std::this_thread::sleep_until(start_time + std::chrono::milliseconds(200));

        m_wrapper.a.store(m_value2);
        m_wrapper.a.notify_all();

        if (!thread1.try_join_for(std::chrono::seconds(5)))
        {
            BOOST_ERROR("Thread 1 failed to join");
            std::abort();
        }

        if (m_wrapper.a.has_native_wait_notify())
        {
            if ((m_thread1_state.m_wakeup_time - start_time) < std::chrono::milliseconds(200))
            {
                std::cout << BOOST_CURRENT_FUNCTION << ": first thread woke up too soon: "
                    << std::chrono::duration_cast< std::chrono::milliseconds >(m_thread1_state.m_wakeup_time - start_time).count() << " ms" << std::endl;
                return false;
            }
        }

        BOOST_TEST_EQ(m_thread1_state.m_received_result.value, m_value2);
        BOOST_TEST(!m_thread1_state.m_received_result.timeout);

        return true;
    }

private:
    void thread_func(thread_state* state)
    {
        m_barrier.arrive_and_wait();

        state->m_received_result = m_wrapper.a.wait_until(m_value1, Clock::now() + std::chrono::seconds(2));
        state->m_wakeup_time = Clock::now();
    }
};

template< template< typename > class Wrapper, typename Clock, typename T >
inline void test_notify_abs_timeout(T value1, T value2)
{
    for (unsigned int i = 0u; i < test_retry_count; ++i)
    {
        // Avoid creating IPC atomics on the stack as this breaks on Darwin
        std::unique_ptr< notify_abs_timeout_test< Wrapper, T, Clock > > test(new notify_abs_timeout_test< Wrapper, T, Clock >(value1, value2));
        if (test->run())
            return;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    BOOST_ERROR("notify_abs_timeout_test could not complete because blocked thread wake up too soon");
}

/*!
 * The test verifies that notifying interrupts a waiting operation with a relative timeout.
 */
template< template< typename > class Wrapper, typename T >
class notify_rel_timeout_test
{
private:
    struct thread_state
    {
        boost::atomics::wait_result< T > m_received_result;
        std::chrono::steady_clock::time_point m_wakeup_time;

        explicit thread_state(T value)
        {
            m_received_result.value = value;
            m_received_result.timeout = true;
        }
    };

private:
    Wrapper< T > m_wrapper;

    char m_padding[1024];

    T m_value1, m_value2;

    test_barrier m_barrier;

    thread_state m_thread1_state;

public:
    explicit notify_rel_timeout_test(T value1, T value2) :
        m_wrapper(value1),
        m_value1(value1),
        m_value2(value2),
        m_barrier(2),
        m_thread1_state(value1)
    {
    }

    bool run()
    {
        test_thread thread1([this]() { this->thread_func(&this->m_thread1_state); });

        m_barrier.arrive_and_wait();

        std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();

        std::this_thread::sleep_until(start_time + std::chrono::milliseconds(200));

        m_wrapper.a.store(m_value2);
        m_wrapper.a.notify_all();

        if (!thread1.try_join_for(std::chrono::seconds(5)))
        {
            BOOST_ERROR("Thread 1 failed to join");
            std::abort();
        }

        if (m_wrapper.a.has_native_wait_notify())
        {
            if ((m_thread1_state.m_wakeup_time - start_time) < std::chrono::milliseconds(200))
            {
                std::cout << BOOST_CURRENT_FUNCTION << ": first thread woke up too soon: "
                    << std::chrono::duration_cast< std::chrono::milliseconds >(m_thread1_state.m_wakeup_time - start_time).count() << " ms" << std::endl;
                return false;
            }
        }

        BOOST_TEST_EQ(m_thread1_state.m_received_result.value, m_value2);
        BOOST_TEST(!m_thread1_state.m_received_result.timeout);

        return true;
    }

private:
    void thread_func(thread_state* state)
    {
        m_barrier.arrive_and_wait();

        state->m_received_result = m_wrapper.a.wait_for(m_value1, std::chrono::seconds(2));
        state->m_wakeup_time = std::chrono::steady_clock::now();
    }
};

template< template< typename > class Wrapper, typename T >
inline void test_notify_rel_timeout(T value1, T value2)
{
    for (unsigned int i = 0u; i < test_retry_count; ++i)
    {
        // Avoid creating IPC atomics on the stack as this breaks on Darwin
        std::unique_ptr< notify_rel_timeout_test< Wrapper, T > > test(new notify_rel_timeout_test< Wrapper, T >(value1, value2));
        if (test->run())
            return;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    BOOST_ERROR("notify_rel_timeout_test could not complete because blocked thread wake up too soon");
}

//! Invokes all wait/notify tests
template< template< typename > class Wrapper, typename T >
void test_wait_notify_api_impl(T value1, T value2, T value3, std::true_type)
{
    test_wait_value_mismatch< Wrapper >(value1, value2);
    test_notify_one< Wrapper >(value1, value2, value3);
    test_notify_all< Wrapper >(value1, value2);
    test_abs_timeout< Wrapper, std::chrono::system_clock >(value1);
    test_abs_timeout< Wrapper, std::chrono::steady_clock >(value1);
    test_rel_timeout< Wrapper >(value1);
    test_notify_abs_timeout< Wrapper, std::chrono::system_clock >(value1, value2);
    test_notify_abs_timeout< Wrapper, std::chrono::steady_clock >(value1, value2);
    test_notify_rel_timeout< Wrapper >(value1, value2);
}

template< template< typename > class Wrapper, typename T >
inline void test_wait_notify_api_impl(T, T, T, std::false_type)
{
}

//! Invokes all wait/notify tests, if the atomic type is lock-free
template< template< typename > class Wrapper, typename T >
inline void test_wait_notify_api(T value1, T value2, T value3)
{
    test_wait_notify_api_impl< Wrapper >(value1, value2, value3, std::integral_constant< bool, Wrapper< T >::atomic_type::is_always_lock_free >());
}

//! Invokes all wait/notify tests, if the atomic type is lock-free
template< template< typename > class Wrapper, typename T >
inline void test_wait_notify_api(T value1, T value2, T value3, int has_native_wait_notify_macro)
{
    BOOST_TEST_EQ(Wrapper< T >::atomic_type::always_has_native_wait_notify, (has_native_wait_notify_macro == 2));
    test_wait_notify_api< Wrapper >(value1, value2, value3);
}


inline void test_flag_wait_notify_api()
{
#if BOOST_ATOMIC_FLAG_LOCK_FREE == 2
    boost::ipc_atomic_flag f = BOOST_ATOMIC_FLAG_INIT;

    bool received_value = f.wait(true);
    BOOST_TEST(!received_value);
    f.notify_one();
    f.notify_all();
#endif // BOOST_ATOMIC_FLAG_LOCK_FREE == 2
}

#endif // BOOST_ATOMIC_TEST_IPC_WAIT_TEST_HELPERS_HPP_INCLUDED_

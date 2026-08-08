// Copyright 2025 Christian Granzin
// Copyright 2010 Christophe Henry
// henry UNDERSCORE christophe AT hotmail DOT com
// This is an extended version of the state machine available in the boost::mpl library
// Distributed under the same license as the original.
// Copyright for the original version:
// Copyright 2005 David Abrahams and Aleksey Gurtovoy. Distributed
// under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MSM_TEST_UTILS_HPP
#define BOOST_MSM_TEST_UTILS_HPP

#include "boost/msm/front/states.hpp"
#include <string>
#include <any>

#include <boost/core/typeinfo.hpp>
#include <boost/msm/backmp11/state_machine.hpp>

#define ASSERT_ZERO(value) BOOST_REQUIRE(value == 0);

#define ASSERT_AND_RESET(value, expected)                                      \
    BOOST_REQUIRE(value == expected);                                          \
    value = 0

#define ASSERT_ONE_AND_RESET(value)                                            \
    BOOST_REQUIRE(value == 1);                                                 \
    value = 0

namespace backmp11 = boost::msm::backmp11;
namespace front = boost::msm::front;

[[maybe_unused]] static std::string to_string(const std::type_info& type)
{
    auto full_name = boost::core::demangled_name(type);
    return full_name.substr(full_name.rfind(':') + 1);
}

template <typename...>
struct is_terminate_state : std::false_type {};

template <typename Base, typename SmPtrPolicy>
struct is_terminate_state<
    front::terminate_state<Base, SmPtrPolicy>>
    : std::true_type {};

template <typename T>
[[maybe_unused]] static std::string to_string()
{
    if constexpr (is_terminate_state<T>::value)
    {
        return "terminate_state";
    }
    else
    {
        return to_string(typeid(T));
    }
}

template <typename FrontEnd, typename Config, typename Derived>
[[maybe_unused]] static std::string to_string(
    const backmp11::state_machine<FrontEnd, Config, Derived>& /*sm*/)
{
    return to_string<FrontEnd>();
}

template <typename T>
[[maybe_unused]] static std::string to_string(const T& /*event*/)
{
    return to_string<T>();
}

[[maybe_unused]] static std::string to_string(const std::any& event)
{
    return to_string(event.type());
}

template <typename T>
[[maybe_unused]] static std::string target_to_string()
{
    if constexpr (!std::is_same_v<T, front::none>)
    {
        return std::string{" -> "} + to_string<T>();
    }
    return {};
}

template <typename T>
[[maybe_unused]] static std::string action_to_string()
{
    if constexpr (!std::is_same_v<T, front::none>)
    {
        return std::string{" / "} + to_string<T>();
    }
    return {};
}

template <typename T>
[[maybe_unused]] static std::string guard_to_string()
{
    if constexpr (!std::is_same_v<T, front::none>)
    {
        return std::string{" [ "} + to_string<T>() + " ]";
    }
    return {};
}

[[maybe_unused]] static std::string to_string(backmp11::process_result result)
{
    using process_result = backmp11::process_result;
    switch (result)
    {
        case process_result::discarded:
            return "discarded";
        case process_result::consumed:
            return "consumed";
        case process_result::rejected:
            return "rejected";
        case process_result::deferred:
            return "deferred";
        default:
            return {};
    }
}

template <typename Source, typename Event, typename Target, typename Action,
            typename Guard, typename StateMachine>
[[maybe_unused]] std::string transition_to_string(const StateMachine& sm)
{
    return to_string(sm) + ": \"" + to_string<Source>() + " + " +
           to_string<Event>() + guard_to_string<Guard>() +
           action_to_string<Action>() + target_to_string<Target>();
}

#endif // BOOST_MSM_TEST_UTILS_HPP

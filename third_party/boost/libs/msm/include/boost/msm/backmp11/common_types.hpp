// Copyright 2025 Christian Granzin
// Copyright 2008 Christophe Henry
// henry UNDERSCORE christophe AT hotmail DOT com
// This is an extended version of the state machine available in the boost::mpl library
// Distributed under the same license as the original.
// Copyright for the original version:
// Copyright 2005 David Abrahams and Aleksey Gurtovoy. Distributed
// under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MSM_BACKMP11_COMMON_TYPES_H
#define BOOST_MSM_BACKMP11_COMMON_TYPES_H

#include <any>
#include <boost/msm/back/common_types.hpp>

namespace boost { namespace msm { namespace backmp11
{

using process_result = back::HandledEnum;

using any_event = std::any;

// Selector for the visit mode.
// Can be active_states or all_states in recursive
// or non-recursive mode.
enum class visit_mode
{
    // State selection (mutually exclusive).
    active_states = 0b001,
    all_states    = 0b010,

    // Traversal mode (not set = non-recursive).
    recursive     = 0b100,

    // All valid combinations.
    active_non_recursive = active_states,
    active_recursive     = active_states | recursive,
    all_non_recursive    = all_states,
    all_recursive        = all_states | recursive
};
constexpr visit_mode operator|(visit_mode lhs, visit_mode rhs)
{
    return static_cast<visit_mode>(
        static_cast<std::underlying_type_t<visit_mode>>(lhs) |
        static_cast<std::underlying_type_t<visit_mode>>(rhs)
    );
}

namespace detail
{

using EventSource = back::EventSourceEnum;
using back::HandledEnum;

constexpr EventSource operator|(EventSource lhs, EventSource rhs)
{
    return static_cast<EventSource>(
        static_cast<std::underlying_type_t<EventSource>>(lhs) |
        static_cast<std::underlying_type_t<EventSource>>(rhs)
    );
}

}

}}} // namespace boost::msm::backmp11

#endif // BOOST_MSM_BACKMP11_COMMON_TYPES_H

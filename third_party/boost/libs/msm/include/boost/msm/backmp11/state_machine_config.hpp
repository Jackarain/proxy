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

#ifndef BOOST_MSM_BACKMP11_STATE_MACHINE_CONFIG_HPP
#define BOOST_MSM_BACKMP11_STATE_MACHINE_CONFIG_HPP

#include <deque>

namespace boost::msm::backmp11
{

namespace detail
{

struct config_tag {};
// Check whether a type is a config.
template <class T>
using is_config = std::is_same<typename T::internal::tag, config_tag>;

} // namespace detail

// Config for the default compile policy
// (runtime over compile time).
struct favor_runtime_speed;

// Config for a compile policy,
// which favors compile time over runtime.
struct favor_compile_time;

// Config for the default context parameter
// (no context).
struct no_context {};

// Config for the default root sm parameter
// (no root sm).
struct no_root_sm {};

// Config for the default fsm parameter
// (local transition owner).
struct local_transition_owner{};

// Config for disabling the event pool.
template <typename T>
struct no_event_container;

using transition_owner [[deprecated("Use local_transition_owner instead")]] =
    local_transition_owner;

// Default state machine config.
struct default_state_machine_config
{
    // Tune characteristics related to
    // compile time, runtime performance, and code size.
    using compile_policy = favor_runtime_speed;
    // A common context that is shared by all SMs
    // in hierarchical state machines.
    using context = no_context;
    // Identifier for the upper-most SM
    // in hierarchical state machines.
    using root_sm = no_root_sm;
    // Type of the Fsm parameter passed in actions and guards.
    using fsm_parameter = local_transition_owner;
    // Which container to use for the event pool.
    template <typename T>
    using event_container = std::deque<T>;

    struct internal
    {
        using tag = detail::config_tag;
    };
};

using state_machine_config = default_state_machine_config;

// Configuration parameters to select how events are dispatched.
namespace dispatch_strategy
{

// Generates a flat fold of inline comparison branches.
// The code can be optimized to a jump table.
// + Best executable size and runtime speed for most compilers.
// + No indirection — fully inlinable.
// - O(n) comparisons in the worst case.
struct flat_fold {};

// Generates an array of function pointers.
// + O(1) dispatch.
// + Slightly better compile times.
// - Indirect call through function pointer — not inlinable.
// - Larger executable size (one pointer per state per event type).
struct function_pointer_array {};

} // namespace dispatch_strategy

} // namespace boost::msm::backmp11

#endif // BOOST_MSM_BACKMP11_STATE_MACHINE_CONFIG_HPP

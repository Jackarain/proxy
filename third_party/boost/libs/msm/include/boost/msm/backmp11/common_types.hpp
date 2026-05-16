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

#ifndef BOOST_MSM_BACKMP11_COMMON_TYPES_HPP
#define BOOST_MSM_BACKMP11_COMMON_TYPES_HPP

#include <cstdint>
#include <optional>

#include <boost/msm/back/common_types.hpp>

namespace boost::msm::back
{

// Bitwise operations for process_result.
// Defined in this header instead of back because type_traits are C++11.
// Defined in the back namespace because the operations have to be in the
// same namespace as HandledEnum.

constexpr HandledEnum operator|(HandledEnum lhs, HandledEnum rhs)
{
    return static_cast<HandledEnum>(
        static_cast<std::underlying_type_t<HandledEnum>>(lhs) |
        static_cast<std::underlying_type_t<HandledEnum>>(rhs)
    );
}

constexpr HandledEnum& operator|=(HandledEnum& lhs, HandledEnum rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

constexpr HandledEnum operator&(HandledEnum lhs, HandledEnum rhs)
{
    return static_cast<HandledEnum>(
        static_cast<std::underlying_type_t<HandledEnum>>(lhs) &
        static_cast<std::underlying_type_t<HandledEnum>>(rhs)
    );
}

constexpr HandledEnum& operator&=(HandledEnum& lhs, HandledEnum rhs)
{
    lhs = lhs & rhs;
    return lhs;
}

} // namespace boost::msm::back

namespace boost::msm::backmp11
{

using process_result = back::HandledEnum;

// flag handling
struct flag_or {};
struct flag_and {};

// Selector for the visit mode.
// Can be active_states or all_states in recursive or non-recursive mode.
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

// Additional info required for event processing.
enum class process_info
{
    direct_call,
    submachine_call,
    event_pool
};

// Bitmask for process result checks.
static constexpr process_result handled_true_or_deferred =
    process_result::HANDLED_TRUE | process_result::HANDLED_DEFERRED;

// Occurrence of an event.
// Event occurrences are placed in an event pool for later processing.
class event_occurrence
{
    using process_fn_t = std::optional<process_result> (*)(
        event_occurrence&, void* /*sm*/, uint16_t /*seq_cnt*/);

  public:
    event_occurrence(process_fn_t process_fn) : m_process_fn(process_fn)
    {
    }

    // Try to process the event.
    // A return value std::nullopt means that the conditions for processing
    // were not given and the event has not been dispatched.
    template <typename StateMachine>
    std::optional<process_result> try_process(StateMachine& sm, uint16_t seq_cnt)
    {
        return m_process_fn(*this, static_cast<void*>(&sm), seq_cnt);
    }

    void mark_for_deletion()
    {
        m_marked_for_deletion = true;
    }

    bool marked_for_deletion() const
    {
        return m_marked_for_deletion;
    }

  private:
    process_fn_t m_process_fn{};
    // Flag set when this event has been processed and can be erased.
    // Deletion is deferred to allow the use of std::deque,
    // which provides better cache locality and lower per-element overhead.
    bool m_marked_for_deletion{};
};

template <typename Event>
class deferred_event : public event_occurrence
{

  public:
    template <typename StateMachine>
    deferred_event(StateMachine&, const Event& event, uint16_t seq_cnt) noexcept
        : event_occurrence(&try_process<StateMachine>), m_seq_cnt(seq_cnt), m_event(event)
    {
    }

    template <typename StateMachine>
    static std::optional<process_result> try_process(event_occurrence& self, void* sm, uint16_t seq_cnt)
    {
        return static_cast<deferred_event*>(&self)
            ->try_process_impl<StateMachine>(*reinterpret_cast<StateMachine*>(sm), seq_cnt);
    }

    template <typename StateMachine>
    std::optional<process_result> try_process_impl(StateMachine& sm, uint16_t seq_cnt)
    {
        if ((m_seq_cnt == seq_cnt) || sm.is_event_deferred(m_event))
        {
            return std::nullopt;
        }
        mark_for_deletion();
        return sm.process_event_internal(
            m_event,
            process_info::event_pool);
    }

  private:
    uint16_t m_seq_cnt;
    Event m_event;
};

template <typename Policy, typename = void>
struct compile_policy_impl;

} // namespace detail
} // namespace boost::msm::backmp11

#endif // BOOST_MSM_BACKMP11_COMMON_TYPES_HPP

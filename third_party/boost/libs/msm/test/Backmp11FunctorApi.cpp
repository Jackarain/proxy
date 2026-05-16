// Copyright 2026 Christian Granzin
// Copyright 2024 Christophe Henry
// henry UNDERSCORE christophe AT hotmail DOT com
// This is an extended version of the state machine available in the boost::mpl library
// Distributed under the same license as the original.
// Copyright for the original version:
// Copyright 2005 David Abrahams and Aleksey Gurtovoy. Distributed
// under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MSM_NONSTANDALONE_TEST
#define BOOST_TEST_MODULE backmp11_completion
#endif
#include <boost/test/unit_test.hpp>

// back-end
#include "Backmp11.hpp"
// #include "BackCommon.hpp"
//front-end
#include "FrontCommon.hpp"
#include "Utils.hpp"

using namespace boost::msm::front;
using namespace boost::msm::backmp11;
namespace mp11 = boost::mp11;

namespace
{

// Events
struct TriggerOnlyFsmActionLambda {};
struct TriggerDefaultAction {};
struct TriggerOnlyEventAndFsmAction {};
struct TriggerOnlyFsmAction {};
struct TriggerActionWithMultipleOverloads {};
struct TriggerSpecificAction {};

// States
struct State1 : test::StateBase
{
};
struct State2 : test::StateBase
{
};

struct Machine_;

// Actions
struct DefaultAction
{
    template <typename Event, typename Fsm, typename Source, typename Target>
    void operator()(const Event&, Fsm&, Source&, Target&)
    {
        static_assert(std::is_same_v<Event, TriggerDefaultAction>);
        static_assert(std::is_base_of_v<Machine_, Fsm>);
        calls += 1;
    }

    [[maybe_unused]] static inline size_t calls{};
};
struct OnlyEventAndFsmAction
{
    template <typename Event, typename Fsm>
    void operator()(const Event&, Fsm&)
    {
        static_assert(std::is_same_v<Event, TriggerOnlyEventAndFsmAction>);
        static_assert(std::is_base_of_v<Machine_, Fsm>);
        calls += 1;
    }

    [[maybe_unused]] static inline size_t calls{};
};
struct OnlyFsmAction
{
    template <typename Fsm>
    void operator()(Fsm&)
    {
        static_assert(std::is_base_of_v<Machine_, Fsm>);
        calls += 1;
    }

    [[maybe_unused]] static inline size_t calls{};
};
struct ActionWithMultipleOverloads : DefaultAction
{
    template <typename Event, typename Fsm, typename Source, typename Target>
    void operator()(const Event&, Fsm&, Source&, Target&)
    {
        static_assert(std::is_same_v<Event, TriggerActionWithMultipleOverloads>);
        static_assert(std::is_base_of_v<Machine_, Fsm>);
        default_calls += 1;
    }
    
    [[maybe_unused]] static inline size_t default_calls{};

    // Overload constraint because caused by priority tags in this edge case:
    // A specific overload must use the highest used arity of the generic overloads.
    template <typename Fsm, typename Source, typename Target>
    void operator()(const TriggerSpecificAction&, Fsm&, Source&, Target&)
    {
        static_assert(std::is_base_of_v<Machine_, Fsm>);
        specific_calls += 1;
    }

    [[maybe_unused]] static inline size_t specific_calls{};

    template <typename Event, typename Fsm>
    void operator()(const Event&, Fsm&)
    {
        static_assert(std::is_same_v<Event, TriggerActionWithMultipleOverloads>);
        static_assert(std::is_base_of_v<Machine_, Fsm>);
        only_event_and_fsm_calls += 1;
    }

    [[maybe_unused]] static inline size_t only_event_and_fsm_calls{};

    template <typename Fsm>
    void operator()(Fsm&)
    {
        static_assert(std::is_base_of_v<Machine_, Fsm>);
        only_fsm_calls += 1;
    }

    [[maybe_unused]] static inline size_t only_fsm_calls{};
};

// Lambda support for shorter syntax (requires C++20).
#if __cplusplus >= 202002L
using OnlyFsmActionLambda = Lambda<[](auto& fsm) {
    using Fsm = std::decay_t<decltype(fsm)>;
    static_assert(std::is_base_of_v<Machine_, Fsm>);
}>;
#endif

// Guards
struct DefaultGuard
{
    template <typename Event, typename Fsm, typename Source, typename Target>
    bool operator()(const Event&, Fsm&, Source&, Target&)
    {
        static_assert(std::is_same_v<Event, TriggerDefaultAction>);
        static_assert(std::is_base_of_v<Machine_, Fsm>);
        calls += 1;
        return true;
    }

    [[maybe_unused]] static inline size_t calls{};
};
struct OnlyEventAndFsmGuard
{
    template <typename Event, typename Fsm>
    bool operator()(const Event&, Fsm&)
    {
        static_assert(std::is_same_v<Event, TriggerOnlyEventAndFsmAction>);
        static_assert(std::is_base_of_v<Machine_, Fsm>);
        calls += 1;
        return true;
    }

    [[maybe_unused]] static inline size_t calls{};
};
struct OnlyFsmGuard
{
    template <typename Fsm>
    bool operator()(Fsm&)
    {
        static_assert(std::is_base_of_v<Machine_, Fsm>);
        calls += 1;
        return true;
    }

    [[maybe_unused]] static inline size_t calls{};
};
struct GuardWithMultipleOverloads
{
    template <typename Event, typename Fsm, typename Source, typename Target>
    bool operator()(const Event&, Fsm&, Source&, Target&)
    {
        static_assert(std::is_same_v<Event, TriggerActionWithMultipleOverloads>);
        static_assert(std::is_base_of_v<Machine_, Fsm>);
        default_calls += 1;
        return true;
    }

    [[maybe_unused]] static inline size_t default_calls{};

    // Overload constraint because caused by priority tags in this edge case:
    // A specific overload must use the highest used arity of the generic overloads.
    template <typename Fsm, typename Source, typename Target>
    bool operator()(const TriggerSpecificAction&, Fsm&, Source&, Target&)
    {
        static_assert(std::is_base_of_v<Machine_, Fsm>);
        specific_calls += 1;
        return true;
    }

    [[maybe_unused]] static inline size_t specific_calls{};

    template <typename Event, typename Fsm>
    bool operator()(const Event&, Fsm&)
    {
        static_assert(std::is_same_v<Event, TriggerActionWithMultipleOverloads>);
        static_assert(std::is_base_of_v<Machine_, Fsm>);
        only_event_and_fsm_calls += 1;
        return true;
    }

    [[maybe_unused]] static inline size_t only_event_and_fsm_calls{};

    template <typename Fsm>
    bool operator()(Fsm&)
    {
        static_assert(std::is_base_of_v<Machine_, Fsm>);
        only_fsm_calls += 1;
        return true;
    }

    [[maybe_unused]] static inline size_t only_fsm_calls{};
};
// Lambda support for shorter syntax (requires C++20).
#if __cplusplus >= 202002L
using OnlyFsmGuardLambda = Lambda<[](auto& fsm) {
    using Fsm = std::decay_t<decltype(fsm)>;
    static_assert(std::is_base_of_v<Machine_, Fsm>);
    return true;
}>;
#endif

struct Machine_ : test::StateMachineBase_<Machine_>
{
    using initial_state = State1;

    using transition_table = mp11::mp_list<
        //    Start   Event                                Next  Action                       Guard
#if __cplusplus >= 202002L
        Row < State1, TriggerOnlyFsmActionLambda        , none, OnlyFsmActionLambda        , OnlyFsmGuardLambda         >,
#endif
        Row < State1, TriggerDefaultAction              , none, DefaultAction              , DefaultGuard               >,
        Row < State1, TriggerOnlyEventAndFsmAction      , none, OnlyEventAndFsmAction      , OnlyEventAndFsmGuard       >,
        Row < State1, TriggerOnlyFsmAction              , none, OnlyFsmAction              , OnlyFsmGuard               >,
        Row < State1, TriggerActionWithMultipleOverloads, none, ActionWithMultipleOverloads, GuardWithMultipleOverloads >,
        Row < State1, TriggerSpecificAction             , none, ActionWithMultipleOverloads, GuardWithMultipleOverloads >
    >;
};

// Pick a back-end
using TestMachines = mp11::mp_list<
#ifndef BOOST_MSM_TEST_SKIP_BACKMP11
    state_machine<Machine_>,
    state_machine<Machine_, favor_compile_time_config>
#endif // BOOST_MSM_TEST_SKIP_BACKMP11
    >;

BOOST_AUTO_TEST_CASE_TEMPLATE(test, TestMachine, TestMachines)
{
    TestMachine state_machine;

    state_machine.start();
    
    state_machine.process_event(TriggerDefaultAction());
    ASSERT_ONE_AND_RESET(DefaultAction::calls);
    ASSERT_ONE_AND_RESET(DefaultGuard::calls);

    state_machine.process_event(TriggerOnlyEventAndFsmAction());
    ASSERT_ONE_AND_RESET(OnlyEventAndFsmAction::calls);
    ASSERT_ONE_AND_RESET(OnlyEventAndFsmGuard::calls);

    state_machine.process_event(TriggerOnlyFsmAction());
    ASSERT_ONE_AND_RESET(OnlyFsmAction::calls);
    ASSERT_ONE_AND_RESET(OnlyFsmGuard::calls);

    state_machine.process_event(TriggerActionWithMultipleOverloads());
    ASSERT_ONE_AND_RESET(ActionWithMultipleOverloads::default_calls);
    ASSERT_ONE_AND_RESET(GuardWithMultipleOverloads::default_calls);

#if __cplusplus >= 202002L
    state_machine.process_event(TriggerOnlyFsmActionLambda());
#endif
}

} // namespace

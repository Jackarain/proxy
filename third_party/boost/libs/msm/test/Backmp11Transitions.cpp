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

#ifndef BOOST_MSM_NONSTANDALONE_TEST
#define BOOST_TEST_MODULE backmp11_transitions_test
#endif
#include <boost/test/unit_test.hpp>

// back-end
#include <boost/msm/backmp11/state_machine.hpp>
#include <boost/msm/backmp11/favor_compile_time.hpp>
//front-end
#include "FrontCommon.hpp"

#include "Utils.hpp"

namespace msm = boost::msm;
namespace mp11 = boost::mp11;

using namespace msm::front;
using namespace msm::backmp11;

namespace
{

// Events.
struct EnterSubmachine{};
struct TriggerTransition{};
struct TriggerInternalTransition{};
struct TriggerInternalTransitionWithGuard
{
    bool guard_value{};
};
struct TriggerSmInternalTransition{};
struct TriggerAnyTransition{};

// Actions
struct MyAction
{
    template<typename Event, typename Fsm, typename Source, typename Target>
    void operator()(const Event&, Fsm&, Source& source, Target&)
    {
        source.action_counter++;
    }
};

// Guards
struct MyGuard
{
    template<typename Fsm>
    bool operator()(const TriggerInternalTransitionWithGuard& event, Fsm&) const
    {
        return event.guard_value;
    }
};

struct NotMyGuard
{
    template<typename Fsm>
    bool operator()(const TriggerInternalTransitionWithGuard& event, Fsm&) const
    {
        return !event.guard_value;
    }
};

// States.
struct MyState : public test::StateBase{};
struct MyOtherState : public test::StateBase{};
class StateMachine;
struct StateMachine_;

struct default_config : state_machine_config
{
    // using root_sm = StateMachine;
    template <typename T>
    using event_container = no_event_container<T>;
};
struct favor_compile_time_config : default_config
{
    using compile_policy = favor_compile_time;
};

template <typename Config = default_config>
struct hierarchical_state_machine
{

struct Submachine_ : public test::StateMachineBase_<Submachine_>
{
    using initial_state = MyState;
    using transition_table = mp11::mp_list<
        Row<MyState, TriggerInternalTransitionWithGuard, none, MyAction, MyGuard>
    >;
};
using Submachine = state_machine<Submachine_, Config>;

struct StateMachine_ : public test::StateMachineBase_<StateMachine_>
{
    using initial_state = MyState;
    using transition_table = mp11::mp_list<
        Row<MyState   , TriggerInternalTransition         , none        , MyAction, none>,
        Row<MyState   , TriggerInternalTransitionWithGuard, none        , MyAction, NotMyGuard>,
        Row<MyState   , TriggerSmInternalTransition       , none        , MyAction, none>,
        Row<MyState   , TriggerTransition                 , MyOtherState, MyAction, none>,
        Row<MyState   , EnterSubmachine                   , Submachine  , none    , none>,
        Row<Submachine, TriggerInternalTransitionWithGuard, Submachine  , MyAction, NotMyGuard>
    >;
    using internal_transition_table = mp11::mp_list<
        Internal<std::any                   , MyAction, none>,
        Internal<TriggerSmInternalTransition, MyAction, none>
    >;
};

// Leave this class without inheriting constructors to check
// that this only needs to be done in case we use a context
// (for convenience).
class StateMachine : public state_machine<StateMachine_, Config, StateMachine>
{
};

};

using TestMachines = boost::mpl::vector<
    hierarchical_state_machine<>,
    hierarchical_state_machine<favor_compile_time_config>
    >;


BOOST_AUTO_TEST_CASE_TEMPLATE(start_stop, StateMachine, TestMachines)
{
    using TestMachine = typename StateMachine::StateMachine;
    TestMachine test_machine;

    test_machine.start();
    BOOST_REQUIRE(test_machine.entry_counter == 1);

    test_machine.stop();
    BOOST_REQUIRE(test_machine.exit_counter == 1);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(transitions, StateMachine, TestMachines)
{
    using TestMachine = typename StateMachine::StateMachine;
    using compile_policy = typename TestMachine::config_t::compile_policy;
    TestMachine test_machine;

    test_machine.start();

    if constexpr (std::is_same_v<compile_policy, favor_runtime_speed>)
    {
        test_machine.process_event(TriggerAnyTransition{});
        ASSERT_ONE_AND_RESET(test_machine.action_counter);
    }

    test_machine.process_event(TriggerInternalTransition{});
    ASSERT_ONE_AND_RESET(test_machine.template get_state<MyState>().action_counter);

    // Ensure MyState consumes the event instead of StateMachine.
    test_machine.process_event(TriggerSmInternalTransition{});
    ASSERT_ONE_AND_RESET(test_machine.template get_state<MyState>().action_counter);


    test_machine.process_event(TriggerTransition{});
    ASSERT_ONE_AND_RESET(test_machine.template get_state<MyState>().action_counter);
    BOOST_REQUIRE(test_machine.template is_state_active<MyOtherState>());

    if constexpr (std::is_same_v<compile_policy, favor_runtime_speed>)
    {
        test_machine.process_event(TriggerAnyTransition{});
        ASSERT_ONE_AND_RESET(test_machine.action_counter);
    }

    test_machine.process_event(TriggerSmInternalTransition{});
    ASSERT_ONE_AND_RESET(test_machine.action_counter);

    test_machine.stop();
}

BOOST_AUTO_TEST_CASE_TEMPLATE(submachine_transitions, StateMachine, TestMachines)
{
    using TestMachine = typename StateMachine::StateMachine;
    TestMachine test_machine;
    auto& submachine = test_machine.template get_state<typename StateMachine::Submachine>();

    test_machine.start();

    test_machine.process_event(EnterSubmachine{});
    ASSERT_ONE_AND_RESET(submachine.entry_counter);

    test_machine.process_event(TriggerInternalTransitionWithGuard{false});
    ASSERT_ONE_AND_RESET(submachine.action_counter);

    test_machine.process_event(TriggerInternalTransitionWithGuard{true});
    ASSERT_ONE_AND_RESET(submachine.template get_state<MyState>().action_counter);    

    test_machine.stop();
}

} // namespace

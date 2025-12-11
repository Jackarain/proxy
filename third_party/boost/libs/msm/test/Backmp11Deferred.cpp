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

// back-end
#include "BackCommon.hpp"
// front-end
#include <boost/msm/front/state_machine_def.hpp>
#include <boost/msm/front/functor_row.hpp>

#ifndef BOOST_MSM_NONSTANDALONE_TEST
#define BOOST_TEST_MODULE backmp11_deferred
#endif
#include <boost/test/unit_test.hpp>

using namespace boost::msm::front;
using namespace boost::msm::backmp11;

namespace mp11 = boost::mp11;

// Events
struct Event1 {};
struct Event2 {};
struct ManualDeferEvent {};
struct FromHandleAllToHandleNone {};
struct FromHandleNoneToHandleAll {};
struct FromDeferEvent1And2ToDeferEvent1 {};
struct FromDeferEvent1ToHandleNone {};

// Actions
struct Action
{
    template <typename Fsm, typename SourceState, typename TargetState>
    void operator()(const Event1&, Fsm& fsm, SourceState&, TargetState&)
    {
        fsm.event1_action_calls++;
    }

    template <typename Fsm, typename SourceState, typename TargetState>
    void operator()(const Event2&, Fsm& fsm, SourceState&, TargetState&)
    {
        fsm.event2_action_calls++;
    }
};

// Common states
struct StateHandleNone : state<> {};
struct StateHandleAll : state<> {};

template<typename T>
struct StateMachineBase_ : state_machine_def<T> 
{
    bool check_and_reset_event1_action_calls(size_t expected)
    {
        const bool result = (event1_action_calls == expected);
        event1_action_calls = 0;
        return result;
    }

    bool check_and_reset_event2_action_calls(size_t expected)
    {
        const bool result = (event2_action_calls == expected);
        event2_action_calls = 0;
        return result;
    }

    size_t event1_action_calls{};
    size_t event2_action_calls{};
};

namespace uml_deferred
{

struct StateDeferEvent1 : state<>
{
    using deferred_events = mp11::mp_list<Event1>;
};

struct StateDeferEvent1And2 : state<>
{
    using deferred_events = mp11::mp_list<Event1, Event2>;
};

struct StateMachine_ : StateMachineBase_<StateMachine_> 
{
    using initial_state =
        mp11::mp_list<StateHandleAll, StateDeferEvent1, StateDeferEvent1And2>;

    using transition_table = mp11::mp_list<
        Row<StateHandleNone      , FromHandleNoneToHandleAll        , StateHandleAll   , none  >,
        Row<StateHandleAll       , FromHandleAllToHandleNone        , StateHandleNone  , none  >,
        Row<StateDeferEvent1And2 , FromDeferEvent1And2ToDeferEvent1 , StateDeferEvent1 , none  >,
        Row<StateDeferEvent1     , FromDeferEvent1ToHandleNone      , StateHandleNone  , none  >,
        Row<StateHandleAll       , Event1                           , none             , Action>,
        Row<StateHandleAll       , Event2                           , none             , Action>,
        Row<StateHandleAll       , ManualDeferEvent                 , none             , Defer  >
    >;
};

// Pick a back-end
using Fsms = mp11::mp_list<
#ifndef BOOST_MSM_TEST_SKIP_BACKMP11
    state_machine<StateMachine_>,
    state_machine<StateMachine_, favor_compile_time_config>
#endif // BOOST_MSM_TEST_SKIP_BACKMP11
    >;


BOOST_AUTO_TEST_CASE_TEMPLATE(uml_deferred, Fsm, Fsms)
{     
    Fsm fsm;

    fsm.start();
    
    fsm.process_event(Event1{});
    BOOST_REQUIRE(fsm.check_and_reset_event1_action_calls(0));
    BOOST_REQUIRE(fsm.check_and_reset_event2_action_calls(0));
    BOOST_REQUIRE(fsm.get_deferred_events_queue().size() == 1);
    
    fsm.process_event(Event2{});
    BOOST_REQUIRE(fsm.check_and_reset_event1_action_calls(0));
    BOOST_REQUIRE(fsm.check_and_reset_event2_action_calls(0));
    BOOST_REQUIRE(fsm.get_deferred_events_queue().size() == 2);
    
    fsm.process_event(FromDeferEvent1And2ToDeferEvent1{});
    BOOST_REQUIRE(fsm.check_and_reset_event1_action_calls(0));
    BOOST_REQUIRE(fsm.check_and_reset_event2_action_calls(1));
    BOOST_REQUIRE(fsm.get_deferred_events_queue().size() == 1);
    
    fsm.process_event(FromDeferEvent1ToHandleNone{});
    BOOST_REQUIRE(fsm.check_and_reset_event1_action_calls(1));
    BOOST_REQUIRE(fsm.check_and_reset_event2_action_calls(0));
    BOOST_REQUIRE(fsm.get_deferred_events_queue().size() == 0);

    fsm.process_event(ManualDeferEvent{});
    BOOST_REQUIRE(fsm.check_and_reset_event1_action_calls(0));
    BOOST_REQUIRE(fsm.check_and_reset_event2_action_calls(0));
    BOOST_REQUIRE(fsm.get_deferred_events_queue().size() == 1);

    // The new event gets deferred: +1
    // The deferred event gets consumed and deferred again: -1, +1
    fsm.process_event(ManualDeferEvent{});
    BOOST_REQUIRE(fsm.check_and_reset_event1_action_calls(0));
    BOOST_REQUIRE(fsm.check_and_reset_event2_action_calls(0));
    BOOST_REQUIRE(fsm.get_deferred_events_queue().size() == 2);

    fsm.stop();
}

} // namespace uml_deferred

// Test case for manual deferral by using transitions with Defer actions.
// Not specified in UML and thus no clear semantics how it should behave.
// Currently a Defer action consumes the event (it gets removed from the queue)
// and then defers it (it gets pushed back to the queue), the Defer action
// returns HANDLED_DEFERRED as processing result).
namespace action_deferred
{

struct StateDeferEvent1 : state<>
{
};

struct StateDeferEvent1And2 : state<>
{
};

struct StateMachine_ : StateMachineBase_<StateMachine_> 
{
    using activate_deferred_events = int;
    using initial_state =
        mp11::mp_list<StateHandleAll, StateDeferEvent1And2>;

    using transition_table = mp11::mp_list<
        Row<StateHandleNone      , FromHandleNoneToHandleAll        , StateHandleAll   , none  >,
        Row<StateHandleAll       , FromHandleAllToHandleNone        , StateHandleNone  , none  >,
        Row<StateDeferEvent1And2 , FromDeferEvent1And2ToDeferEvent1 , StateDeferEvent1 , none  >,
        Row<StateDeferEvent1     , FromDeferEvent1ToHandleNone      , StateHandleNone  , none  >,
        Row<StateDeferEvent1     , Event1                           , none             , Defer >,
        Row<StateDeferEvent1And2 , Event1                           , none             , Defer >,
        Row<StateDeferEvent1And2 , Event2                           , none             , Defer >,
        Row<StateHandleAll       , Event1                           , none             , Action>,
        Row<StateHandleAll       , Event2                           , none             , Action>
    >;
};

// Pick a back-end
using Fsms = mp11::mp_list<
#ifndef BOOST_MSM_TEST_SKIP_BACKMP11
    state_machine<StateMachine_>,
    state_machine<StateMachine_, favor_compile_time_config>
#endif // BOOST_MSM_TEST_SKIP_BACKMP11
    >;


BOOST_AUTO_TEST_CASE_TEMPLATE(action_deferred, Fsm, Fsms)
{     
    Fsm fsm;

    fsm.start();
    
    fsm.process_event(Event1{});
    // Processed by StateHandleAll, deferred by StateDeferEvent1And2.
    // Queue: Event1
    BOOST_REQUIRE(fsm.check_and_reset_event1_action_calls(1));
    BOOST_REQUIRE(fsm.get_deferred_events_queue().size() == 1);
    
    fsm.process_event(Event2{});
    // Processed by StateHandleAll, deferred by StateDeferEvent1And2.
    // StateHandleAll also processes Event1 again.
    // Queue: Event2, Event1
    BOOST_REQUIRE(fsm.check_and_reset_event1_action_calls(1));
    BOOST_REQUIRE(fsm.check_and_reset_event2_action_calls(1));
    BOOST_REQUIRE(fsm.get_deferred_events_queue().size() == 2);
    
    fsm.process_event(FromDeferEvent1And2ToDeferEvent1{});
    // Event2 is no more deferred.
    // StateHandleAll processes Event2 & Event1,
    // StateDeferEvent1 defers Event1.
    // Queue: Event1
    BOOST_REQUIRE(fsm.check_and_reset_event1_action_calls(1));
    BOOST_REQUIRE(fsm.check_and_reset_event2_action_calls(1));
    BOOST_REQUIRE(fsm.get_deferred_events_queue().size() == 1);
    
    fsm.process_event(FromDeferEvent1ToHandleNone{});
    // Event1 is no more deferred.
    // StateHandleAll processes Event1.
    BOOST_REQUIRE(fsm.check_and_reset_event1_action_calls(1));
    BOOST_REQUIRE(fsm.get_deferred_events_queue().size() == 0);

    fsm.stop();
}

} // namespace action_deferred

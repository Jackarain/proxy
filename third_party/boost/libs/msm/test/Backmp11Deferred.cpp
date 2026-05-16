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
#define BOOST_TEST_MODULE backmp11_deferred
#endif
#include <boost/test/unit_test.hpp>

// back-end
#include "BackCommon.hpp"
// front-end
#include "FrontCommon.hpp"

#include "Utils.hpp"

using namespace boost::msm::front;
using namespace boost::msm::backmp11;

namespace mp11 = boost::mp11;

namespace
{

// Events
struct Event1
{
    size_t id{};
};
struct Event2
{
    size_t id{};
};
struct Event3
{
    bool marked_for_deferral{false};
    size_t id{};
};
struct FromHandleAllToHandleNone {};
struct FromHandleNoneToHandleAll {};
struct FromDeferAllToDeferEvent1 {};
struct FromDeferEvent1ToHandleNone {};
struct FromDeferEvent1ToHandleAll {};

// Actions
struct Action
{
    template <typename Fsm, typename SourceState, typename TargetState>
    void operator()(const Event1& event, Fsm& fsm, SourceState&, TargetState&)
    {
        fsm.event1_action_calls++;
        fsm.event1_processed_ids.push_back(event.id);
    }

    template <typename Fsm, typename SourceState, typename TargetState>
    void operator()(const Event2& event, Fsm& fsm, SourceState&, TargetState&)
    {
        fsm.event2_action_calls++;
        fsm.event2_processed_ids.push_back(event.id);
    }

    template <typename Fsm, typename SourceState, typename TargetState>
    void operator()(const Event3& event, Fsm& fsm, SourceState&, TargetState&)
    {
        fsm.event3_action_calls++;
        fsm.event3_processed_ids.push_back(event.id);
    }
};

// Common states
struct StateHandleNone : state<> {};
struct StateHandleAll : state<> {};

template<typename T>
struct StateMachineBase_ : test::StateMachineBase_<T>
{
    size_t event1_action_calls{};
    std::vector<size_t> event1_processed_ids;
    size_t event2_action_calls{};
    std::vector<size_t> event2_processed_ids;
    size_t event3_action_calls{};
    std::vector<size_t> event3_processed_ids;
};

// Test event deferral with the deferred_events property.
namespace property_deferred
{

struct StateDeferEvent1 : state<>
{
    using deferred_events = mp11::mp_list<Event1>;
};

struct StateDeferAll : state<>
{
    using deferred_events = mp11::mp_list<Event1, Event2, Event3>;
};

struct StateDeferEvent3Conditionally : state<>
{
    using deferred_events = mp11::mp_list<Event3>;

    template <typename Fsm>
    bool is_event_deferred(const Event3& event, Fsm&) const
    {
        return event.marked_for_deferral;
    }
};

struct StateMachine_ : StateMachineBase_<StateMachine_> 
{
    using initial_state =
        mp11::mp_list<StateDeferEvent3Conditionally, StateDeferEvent1, StateDeferAll>;

    using transition_table = mp11::mp_list<
        Row<StateDeferAll                 , FromDeferAllToDeferEvent1   , StateDeferEvent1 , none  >,
        Row<StateDeferEvent1              , FromDeferEvent1ToHandleNone , StateHandleNone  , none  >,
        Row<StateDeferEvent3Conditionally , Event1                      , none             , Action>,
        Row<StateDeferEvent3Conditionally , Event2                      , none             , Action>,
        Row<StateDeferEvent3Conditionally , Event3                      , none             , Action>
    >;
};

// Pick a back-end
using Fsms = mp11::mp_list<
#ifndef BOOST_MSM_TEST_SKIP_BACKMP11
    StateMachine<StateMachine_>,
    StateMachine<StateMachine_, favor_compile_time_config>
#endif // BOOST_MSM_TEST_SKIP_BACKMP11
    >;


BOOST_AUTO_TEST_CASE_TEMPLATE(property_deferred, Fsm, Fsms)
{     
    Fsm fsm;

    fsm.start();
    
    fsm.process_event(Event1{0});
    ASSERT_AND_RESET(fsm.event1_action_calls, 0);
    ASSERT_AND_RESET(fsm.event2_action_calls, 0);
    BOOST_REQUIRE(fsm.get_pending_events().size() == 1);
    
    fsm.process_event(Event1{1});
    ASSERT_AND_RESET(fsm.event1_action_calls, 0);
    ASSERT_AND_RESET(fsm.event2_action_calls, 0);
    BOOST_REQUIRE(fsm.get_pending_events().size() == 2);

    fsm.process_event(Event2{0});
    ASSERT_AND_RESET(fsm.event1_action_calls, 0);
    ASSERT_AND_RESET(fsm.event2_action_calls, 0);
    BOOST_REQUIRE(fsm.get_pending_events().size() == 3);

    // StateDeferEvent3Conditionally would process,
    // but StateDeferAll defers the event.
    fsm.process_event(Event3{false, 0});
    ASSERT_AND_RESET(fsm.event1_action_calls, 0);
    ASSERT_AND_RESET(fsm.event2_action_calls, 0);
    ASSERT_AND_RESET(fsm.event3_action_calls, 0);
    BOOST_REQUIRE(fsm.get_pending_events().size() == 4);
    
    fsm.process_event(FromDeferAllToDeferEvent1{});
    ASSERT_AND_RESET(fsm.event1_action_calls, 0);
    ASSERT_AND_RESET(fsm.event2_action_calls, 1);
    ASSERT_AND_RESET(fsm.event3_action_calls, 1);
    BOOST_REQUIRE(fsm.event3_processed_ids.at(0) == 0);
    BOOST_REQUIRE(fsm.get_pending_events().size() == 2);
    
    fsm.process_event(FromDeferEvent1ToHandleNone{});
    ASSERT_AND_RESET(fsm.event1_action_calls, 2);
    BOOST_REQUIRE(fsm.event1_processed_ids.at(0) == 0);
    BOOST_REQUIRE(fsm.event1_processed_ids.at(1) == 1);
    ASSERT_AND_RESET(fsm.event2_action_calls, 0);
    BOOST_REQUIRE(fsm.get_pending_events().size() == 0);

    // The event gets conditionally deferred.
    fsm.process_event(Event3{true, 1});
    ASSERT_AND_RESET(fsm.event1_action_calls, 0);
    ASSERT_AND_RESET(fsm.event2_action_calls, 0);
    BOOST_REQUIRE(fsm.get_pending_events().size() == 1);

    // The previous event stays conditionally deferred,
    // the new event gets consumed.
    fsm.process_event(Event3{false, 2});
    ASSERT_AND_RESET(fsm.event1_action_calls, 0);
    ASSERT_AND_RESET(fsm.event2_action_calls, 0);
    ASSERT_AND_RESET(fsm.event3_action_calls, 1);
    BOOST_REQUIRE(fsm.event3_processed_ids.at(1) == 2);
    BOOST_REQUIRE(fsm.get_pending_events().size() == 1);

    fsm.stop();
}

struct FromLowerMachineToHandleAll {};

template <typename Config = default_state_machine_config>
struct hierarchical_state_machine
{
    struct LowerMachine_;
    struct UpperMachine_;

    struct StateDeferEvent1 : state<>
    {
        using deferred_events = mp11::mp_list<Event1>;

        template <typename Fsm>
        bool is_event_deferred(const Event1&, Fsm&) const
        {
            static_assert(std::is_base_of_v<LowerMachine_, Fsm>);
            return true;
        }
    };

    struct LowerMachine_ : StateMachineBase_<LowerMachine_> 
    {
        using initial_state = mp11::mp_list<StateDeferEvent1>;
    };
    using LowerMachine = StateMachine<LowerMachine_, Config>;

    struct UpperMachine_ : StateMachineBase_<UpperMachine_> 
    {
        using initial_state = mp11::mp_list<LowerMachine>;

        using transition_table = mp11::mp_list<
            Row<LowerMachine   , FromLowerMachineToHandleAll , StateHandleAll          >,
            Row<StateHandleAll , Event1                      , none           , Action >
        >;
    };
    using UpperMachine = StateMachine<UpperMachine_, Config>;
};

// Pick a back-end
using Fsms_2 = mp11::mp_list<
#ifndef BOOST_MSM_TEST_SKIP_BACKMP11
    hierarchical_state_machine<>,
    hierarchical_state_machine<favor_compile_time_config>
#endif // BOOST_MSM_TEST_SKIP_BACKMP11
    >;

// Ensure an event gets deferred if there
// is no immediate substate that defers the event,
// but a subsubstate in the hierarchy defers it.
BOOST_AUTO_TEST_CASE_TEMPLATE(hierarchical_deferral_subsubstate, Fsm, Fsms_2)
{     
    using UpperMachine = typename Fsm::UpperMachine;
    
    UpperMachine fsm;

    fsm.start();
    
    fsm.process_event(Event1{0});
    ASSERT_AND_RESET(fsm.event1_action_calls, 0);
    BOOST_REQUIRE(fsm.get_pending_events().size() == 1);
    
    fsm.process_event(FromLowerMachineToHandleAll{});
    ASSERT_AND_RESET(fsm.event1_action_calls, 1);
    BOOST_REQUIRE(fsm.get_pending_events().size() == 0);

    fsm.stop();
}


struct FromLowerMachineToDeferEvent1 {};

template <typename Config = default_state_machine_config>
struct hierarchical_state_machine_2
{
    struct LowerMachine_;
    struct UpperMachine_;

    struct StateDeferEvent1 : state<>
    {
        using deferred_events = mp11::mp_list<Event1>;

        template <typename Fsm>
        bool is_event_deferred(const Event1&, Fsm&) const
        {
            if (id == 0)
            {
                BOOST_REQUIRE((std::is_base_of_v<LowerMachine_, Fsm>));
            }
            else
            {
                BOOST_REQUIRE((std::is_base_of_v<UpperMachine_, Fsm>));
            }
            return true;
        }

        size_t id{};
    };

    struct LowerMachine_ : StateMachineBase_<LowerMachine_> 
    {
        using initial_state = mp11::mp_list<StateDeferEvent1>;

        using transition_table = mp11::mp_list<
            Row<StateDeferEvent1 , FromDeferEvent1ToHandleAll , StateHandleAll            >
        >;
    };
    using LowerMachine = StateMachine<LowerMachine_, Config>;

    struct UpperMachine_ : StateMachineBase_<UpperMachine_> 
    {
        using initial_state = mp11::mp_list<LowerMachine>;

        using transition_table = mp11::mp_list<
            Row<LowerMachine     , FromLowerMachineToDeferEvent1 , StateDeferEvent1        >,
            Row<StateDeferEvent1 , FromDeferEvent1ToHandleAll    , StateHandleAll          >,
            Row<StateHandleAll   , Event1                        , none           , Action >
        >;
    };
    using UpperMachine = StateMachine<UpperMachine_, Config>;
};

// Pick a back-end
using Fsms_3 = mp11::mp_list<
#ifndef BOOST_MSM_TEST_SKIP_BACKMP11
    hierarchical_state_machine_2<>,
    hierarchical_state_machine_2<favor_compile_time_config>
#endif // BOOST_MSM_TEST_SKIP_BACKMP11
    >;

// Ensure that the Fsm parameter passed to is_event_deferred is correct.
BOOST_AUTO_TEST_CASE_TEMPLATE(hierarchical_deferral_fsm_parameter, Fsm, Fsms_3)
{     
    using UpperMachine = typename Fsm::UpperMachine;
    
    UpperMachine fsm;
    fsm.template get_state<typename Fsm::StateDeferEvent1>().id = 1;

    fsm.start();
    
    fsm.process_event(Event1{0});
    ASSERT_AND_RESET(fsm.event1_action_calls, 0);
    BOOST_REQUIRE(fsm.get_pending_events().size() == 1);
    
    fsm.process_event(FromLowerMachineToDeferEvent1{});
    ASSERT_AND_RESET(fsm.event1_action_calls, 0);
    BOOST_REQUIRE(fsm.get_pending_events().size() == 1);

    fsm.process_event(Event1{1});
    ASSERT_AND_RESET(fsm.event1_action_calls, 0);
    BOOST_REQUIRE(fsm.get_pending_events().size() == 2);

    fsm.process_event(FromDeferEvent1ToHandleAll{});
    ASSERT_AND_RESET(fsm.event1_action_calls, 2);
    BOOST_REQUIRE(fsm.get_pending_events().size() == 0);
    BOOST_REQUIRE(fsm.event1_processed_ids.at(0) == 0);
    BOOST_REQUIRE(fsm.event1_processed_ids.at(1) == 1);

    fsm.stop();
}

} // namespace property_deferred

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

struct StateDeferAll : state<>
{
};

struct StateMachine_ : StateMachineBase_<StateMachine_> 
{
    using activate_deferred_events = int;
    using initial_state =
        mp11::mp_list<StateHandleAll, StateDeferAll>;

    using transition_table = mp11::mp_list<
        Row<StateDeferAll    , FromDeferAllToDeferEvent1   , StateDeferEvent1 , none  >,
        Row<StateDeferEvent1 , FromDeferEvent1ToHandleNone , StateHandleNone  , none  >,
        Row<StateDeferEvent1 , Event1                      , none             , Defer >,
        Row<StateDeferAll    , Event1                      , none             , Defer >,
        Row<StateDeferAll    , Event2                      , none             , Defer >,
        Row<StateHandleAll   , Event1                      , none             , Action>,
        Row<StateHandleAll   , Event2                      , none             , Action>
    >;
};

// Pick a back-end
using Fsms = mp11::mp_list<
#ifndef BOOST_MSM_TEST_SKIP_BACKMP11
    StateMachine<StateMachine_>,
    StateMachine<StateMachine_, favor_compile_time_config>
#endif // BOOST_MSM_TEST_SKIP_BACKMP11
    >;


BOOST_AUTO_TEST_CASE_TEMPLATE(action_deferred, Fsm, Fsms)
{     
    Fsm fsm;

    fsm.start();
    
    fsm.process_event(Event1{});
    // Processed by StateHandleAll, deferred by StateDeferAll.
    // Queue: Event1
    ASSERT_AND_RESET(fsm.event1_action_calls, 1);
    BOOST_REQUIRE(fsm.get_pending_events().size() == 1);
    
    fsm.process_event(Event2{});
    // Processed by StateHandleAll, deferred by StateDeferAll.
    // StateHandleAll processes Event1 a 2nd time.
    // Queue: Event2, Event1
    ASSERT_AND_RESET(fsm.event1_action_calls, 1);
    ASSERT_AND_RESET(fsm.event2_action_calls, 1);
    BOOST_REQUIRE(fsm.get_pending_events().size() == 2);
    
    fsm.process_event(FromDeferAllToDeferEvent1{});
    // Event2 is no more deferred.
    // StateHandleAll processes Event2 a 2nd time & Event1 a 3rd time,
    // StateDeferEvent1 defers Event1.
    // Queue: Event1
    ASSERT_AND_RESET(fsm.event1_action_calls, 1);
    ASSERT_AND_RESET(fsm.event2_action_calls, 1);
    BOOST_REQUIRE(fsm.get_pending_events().size() == 1);
    
    fsm.process_event(FromDeferEvent1ToHandleNone{});
    // Event1 is no more deferred.
    // StateHandleAll processes Event1 a 4th time.
    ASSERT_AND_RESET(fsm.event1_action_calls, 1);
    BOOST_REQUIRE(fsm.get_pending_events().size() == 0);

    fsm.stop();
}

} // namespace action_deferred

} // namespace

// Copyright 2024 Christophe Henry
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
//front-end
#include <boost/msm/front/functor_row.hpp>
#include <boost/msm/front/state_machine_def.hpp>
#ifndef BOOST_MSM_NONSTANDALONE_TEST
#define BOOST_TEST_MODULE backmp11_many_defer_transitions
#endif
#include <boost/test/unit_test.hpp>

using namespace boost::msm::front;
using namespace boost::msm::backmp11;
namespace mp11 = boost::mp11;

// Events
struct Event1 {};
struct Event2 {};
struct Event3 {};
struct Event4 {};

namespace uml_defer_transitions
{

// States
struct State1 : state<>
{
    using deferred_events = mp11::mp_list<Event1, Event2, Event3>;
};
struct State2 : state<>
{
    using deferred_events = mp11::mp_list<Event1, Event3>;
};
struct State3 : state<>
{
};
struct State4 : state<>
{
};
struct State5 : state<>
{
};

// ----- State machine
struct Sm1_ : state_machine_def<Sm1_>
{
    // Set initial state
    typedef State1 initial_state;
    // Enable deferred capability
    typedef int activate_deferred_events;
    // Transition table
    using transition_table = mp11::mp_list<
        //    Start   Event   Next    Action
        Row < State1, Event4, State2, none >,

        Row < State2, Event2, State3, none >,

        Row < State3, Event1, State4, none >,
        Row < State3, Event3, State5, none >,
        Row < State4, Event3, State5, none >,
        Row < State5, Event1, State4, none >
    >;
};

// Pick a back-end
using TestMachines = mp11::mp_list<
#ifndef BOOST_MSM_TEST_SKIP_BACKMP11
    state_machine<Sm1_>,
    state_machine<Sm1_, favor_compile_time_config>
#endif // BOOST_MSM_TEST_SKIP_BACKMP11
    >;

BOOST_AUTO_TEST_CASE_TEMPLATE(uml_defer_transitions, TestMachine, TestMachines)
{
    TestMachine state_machine;
    state_machine.start();
    
    state_machine.process_event(Event1());
    // Queue: Event1
    BOOST_REQUIRE(state_machine.get_deferred_events_queue().size() == 1);
    
    state_machine.process_event(Event2());
    // Queue: Event1, Event2
    BOOST_REQUIRE(state_machine.get_deferred_events_queue().size() == 2);
    
    state_machine.process_event(Event3());
    // Queue: Event1, Event2, Event3
    BOOST_REQUIRE(state_machine.get_deferred_events_queue().size() == 3);
    
    state_machine.process_event(Event4());
    // Event4 gets consumed, new state is State2.
    // Event1 stays deferred (queue: Event1, Event2, Event3)
    // Event2 gets consumed, new state is State3 (queue: Event1, Event3)
    // Event1 gets consumed, new state is State4 (queue: Event3)
    // Event3 gets consumed, new state is State5
    BOOST_REQUIRE(state_machine.get_deferred_events_queue().size() == 0);
    BOOST_REQUIRE(state_machine.template is_state_active<State5>());
}

} // namespace uml_defer_transitions

// Test case for manual deferral by using transitions with Defer actions.
// Not specified in UML and thus no clear semantics how it should behave.
// Currently a Defer action consumes the event (it gets removed from the queue)
// and then defers it (it gets pushed back to the queue), the Defer action
// returns HANDLED_DEFERRED as processing result).
namespace action_defer_transitions
{

// States
struct State1 : state<>
{
};
struct State2 : state<>
{
};
struct State3 : state<>
{
};
struct State4 : state<>
{
};
struct State5 : state<>
{
};

// ----- State machine
struct Sm1_ : state_machine_def<Sm1_>
{
    // Set initial state
    typedef State1 initial_state;
    // Enable deferred capability
    typedef int activate_deferred_events;
    // Transition table
    using transition_table = mp11::mp_list<
        //    Start   Event   Next    Action
        Row < State1, Event1, none  , Defer>,
        Row < State1, Event2, none  , Defer>,
        Row < State1, Event3, none  , Defer>,
        Row < State1, Event4, State2, none >,

        Row < State2, Event3, none  , Defer>,
        Row < State2, Event1, none  , Defer>,
        Row < State2, Event2, State3, none >,

        Row < State3, Event1, State4, none >,
        Row < State3, Event3, State5, none >,
        Row < State4, Event3, State5, none >,
        Row < State5, Event1, State4, none >
    >;
};

// Pick a back-end
using TestMachines = mp11::mp_list<
#ifndef BOOST_MSM_TEST_SKIP_BACKMP11
    state_machine<Sm1_>,
    state_machine<Sm1_, favor_compile_time_config>
#endif // BOOST_MSM_TEST_SKIP_BACKMP11
    >;

BOOST_AUTO_TEST_CASE_TEMPLATE(action_defer_transitions, TestMachine, TestMachines)
{
    TestMachine state_machine;
    state_machine.start();
    
    state_machine.process_event(Event1());
    // Queue: Event1
    BOOST_REQUIRE(state_machine.get_deferred_events_queue().size() == 1);
    
    state_machine.process_event(Event2());
    // Queue: Event2, Event1
    BOOST_REQUIRE(state_machine.get_deferred_events_queue().size() == 2);
    
    state_machine.process_event(Event3());
    // Queue: Event3, Event2, Event1
    BOOST_REQUIRE(state_machine.get_deferred_events_queue().size() == 3);
    
    state_machine.process_event(Event4());
    // Event4 gets consumed, new state is State2.
    // Event3 gets deferred (queue: Event2, Event1, Event3)
    // Event2 gets consumed, new state is State3 (queue: Event1, Event3)
    // Event1 gets consumed, new state is State4 (queue: Event3)
    BOOST_REQUIRE(state_machine.get_deferred_events_queue().size() == 1);
    BOOST_REQUIRE(state_machine.template is_state_active<State4>());
}

} // namespace action_defer_transitions

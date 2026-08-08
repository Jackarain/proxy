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
#define BOOST_TEST_MODULE orthogonal_deferred4_test
#endif
#include <boost/test/unit_test.hpp>

// back-end
#include "BackCommon.hpp"
//front-end
#include "FrontCommon.hpp"

namespace mpl = boost::mpl;
using namespace boost::msm::front;

namespace
{

// Events
struct Ping {};

// Actions
struct Action
{
    template <typename Event, typename Fsm, typename Source, typename Target>
    void operator()(const Event&, Fsm& fsm, Source&, Target&)
    {
        fsm.action_counter++;
    }
};

// Guards
struct Conditional
{
    template <typename Event, typename Fsm, typename Source, typename Target>
    bool operator()(const Event&, Fsm& fsm, Source&, Target&)
    {
        return fsm.conditional;
    }
};

struct Machine_ : test::StateMachineBase_<Machine_>
{
    typedef int activate_deferred_events;

    struct Deferring : state<> {};
    struct Acting    : state<> {};

    // Two orthogonal regions
    typedef mpl::vector<Deferring, Acting> initial_state;

    using transition_table = mpl::vector<
        //   Source     Event  Target  Action   Guard
        Row< Deferring, Ping,  none,   Defer ,  none>,       // region 1: defer
        Row< Acting   , Ping,  none,   Action,  Conditional> // region 2: handle conditionally
    >;

    size_t action_counter{};
    bool conditional{};
};

using TestMachines = get_test_machines<Machine_>;

BOOST_AUTO_TEST_CASE_TEMPLATE(guard_reject_test, test_machine, TestMachines)
{
    test_machine sm;
    sm.start(); 

    sm.process_event(Ping{});
    BOOST_REQUIRE(sm.action_counter == 0);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(handle_discard_test, TestMachine, TestMachines)
{
    TestMachine sm;
    sm.conditional = true;
    sm.start();

    sm.process_event(Ping{});
    // Action called one times too many in old-backends
    // (though behavior exists since 1.78).
    size_t expected_action_counter = 2;
#ifndef BOOST_MSM_TEST_SKIP_BACKMP11
    if (boost::msm::backmp11::is_backmp11_state_machine<TestMachine>::value)
    {
        expected_action_counter = 1;
    }
#endif
    BOOST_REQUIRE(sm.action_counter == expected_action_counter);
}

} // nmespace

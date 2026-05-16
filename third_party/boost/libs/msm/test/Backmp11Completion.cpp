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
struct InternalTransition {};
struct SelfTransition {};

// States
struct State1 : test::StateBase
{
};
struct State2 : test::StateBase
{
};

// Guards
struct ConditionIsTrue
{
    template <typename Event, typename Fsm, typename... Ts>
    bool operator()(const Event&, Fsm& fsm, Ts...)
    {
        fsm.completion_transition_guard_calls++;
        return fsm.condition;
    }
};

struct Machine_ : test::StateMachineBase_<Machine_>
{
    using initial_state = State1;

    using transition_table = mp11::mp_list<
        //    Start   Event               Next    Action  Guard
        Row < State1, none              , State2, none  , ConditionIsTrue >,
        Row < State1, InternalTransition, none                            >,
        Row < State1, SelfTransition    , State1                          >
    >;

    bool condition{};
    size_t completion_transition_guard_calls{};
};

namespace simple_states
{

// Pick a back-end
using TestMachines = mp11::mp_list<
#ifndef BOOST_MSM_TEST_SKIP_BACKMP11
    state_machine<Machine_>,
    state_machine<Machine_, favor_compile_time_config>
#endif // BOOST_MSM_TEST_SKIP_BACKMP11
    >;

BOOST_AUTO_TEST_CASE_TEMPLATE(simple_states, TestMachine, TestMachines)
{
    TestMachine state_machine;

    state_machine.start();
    ASSERT_ONE_AND_RESET(state_machine.completion_transition_guard_calls);
    
    state_machine.process_event(InternalTransition());
    BOOST_REQUIRE(state_machine.completion_transition_guard_calls == 0);

    state_machine.process_event(SelfTransition());
    ASSERT_ONE_AND_RESET(state_machine.completion_transition_guard_calls);
    state_machine.condition = true;
    state_machine.process_event(SelfTransition());
    ASSERT_ONE_AND_RESET(state_machine.completion_transition_guard_calls);
    BOOST_REQUIRE(state_machine.template is_state_active<State2>());
}

} // namespace simple_states

namespace composite_states
{

template <typename Config = default_state_machine_config>
struct hierarchical_machine
{
    using Machine = state_machine<Machine_, Config>;

    struct UpperMachine_ : test::StateMachineBase_<UpperMachine_>
    {
        using initial_state = Machine;

        using transition_table = mp11::mp_list<
            //    Start    Event  Next  Action  Guard
            Row < Machine, none , none, none  , ConditionIsTrue >
        >;

        bool condition{};
        size_t completion_transition_guard_calls{};
    };

    using UpperMachine = state_machine<UpperMachine_, Config>;
};

// Pick a back-end
using TestMachines = mp11::mp_list<
#ifndef BOOST_MSM_TEST_SKIP_BACKMP11
    hierarchical_machine<>,
    hierarchical_machine<favor_compile_time_config>
#endif // BOOST_MSM_TEST_SKIP_BACKMP11
    >;

BOOST_AUTO_TEST_CASE_TEMPLATE(simple_states, TestMachine, TestMachines)
{
    typename TestMachine::UpperMachine state_machine;

    state_machine.start();
    // The completion of a composite state entry shall not fire a completion transition.
    BOOST_REQUIRE(state_machine.completion_transition_guard_calls == 0);
}


}

namespace orthogonal_regions
{

struct State1c : test::StateBase {};
struct State2c : test::StateBase {};

struct Machine_ : test::StateMachineBase_<Machine_>
{

    using initial_state = mp11::mp_list<State1, State2>;

    using transition_table = mp11::mp_list<
        //    Start   Event   Next    
        Row < State1, none  , State1c >,
        Row < State2, none  , State2c >
    >;
};

// Pick a back-end
using TestMachines = mp11::mp_list<
#ifndef BOOST_MSM_TEST_SKIP_BACKMP11
    state_machine<Machine_>,
    state_machine<Machine_, favor_compile_time_config>
#endif // BOOST_MSM_TEST_SKIP_BACKMP11
    >;

BOOST_AUTO_TEST_CASE_TEMPLATE(orthogonal_regions, TestMachine, TestMachines)
{
    TestMachine state_machine;

    state_machine.start();
    BOOST_REQUIRE(state_machine.template is_state_active<State1c>());
    BOOST_REQUIRE(state_machine.get_active_state_ids()[0] ==
                  state_machine.template get_state_id<State1c>());
    BOOST_REQUIRE(state_machine.template is_state_active<State2c>());
    BOOST_REQUIRE(state_machine.get_active_state_ids()[1] ==
                  state_machine.template get_state_id<State2c>());
}

} // namespace orthogonal_regions

} // namespace

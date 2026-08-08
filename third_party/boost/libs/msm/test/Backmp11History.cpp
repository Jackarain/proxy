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
#define BOOST_TEST_MODULE backmp11_history_test
#endif
#include <boost/test/unit_test.hpp>

// back-end
#include "Backmp11.hpp"
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
struct EnterSubmachine {};
struct EnterSubmachineWithoutHistory {};
struct ExitSubmachine {};
struct EnterSubstate1 {};
struct DeferredEvent {};

// States.
struct State0 : public test::StateBase {};
struct Substate0 : public test::StateBase
{
    using deferred_events = mp11::mp_list<DeferredEvent>;
};
struct Substate1 : public test::StateBase
{
    using deferred_events = mp11::mp_list<DeferredEvent>;
};

struct default_config : state_machine_config
{
};
struct favor_compile_time_config : default_config
{
    using compile_policy = favor_compile_time;
};

template <typename History, typename Config = default_config>
struct hierarchical_state_machine
{
    struct Submachine_ : test::StateMachineBase_<Submachine_>
    {
        using history = History;

        using initial_state = Substate0;

        using transition_table = mp11::mp_list<
            Row<Substate0, EnterSubstate1, Substate1>,
            Row<Substate0, DeferredEvent , none     >
        >;
    };
    using Submachine = StateMachine<Submachine_, Config>;

    struct UpperMachine_ : test::StateMachineBase_<UpperMachine_>
    {
        using initial_state = State0;
        
        using transition_table = mp11::mp_list<
            Row<State0    , EnterSubmachine              , Submachine>,
            Row<State0    , EnterSubmachineWithoutHistory, Submachine>,
            Row<Submachine, ExitSubmachine               , State0    >
        >;
    };
    using UpperMachine = StateMachine<UpperMachine_, Config>;
};

using TestMachinesAlwaysShallowHistory = boost::mpl::vector<
    hierarchical_state_machine<always_shallow_history>,
    hierarchical_state_machine<always_shallow_history, favor_compile_time_config>
    >;


BOOST_AUTO_TEST_CASE_TEMPLATE(always_shallow_history, TestMachine, TestMachinesAlwaysShallowHistory)
{
    using StateMachine = typename TestMachine::UpperMachine;
    using Submachine = typename TestMachine::Submachine;
    StateMachine state_machine;
    auto& submachine = state_machine.template get_state<Submachine>();

    state_machine.start();
    BOOST_REQUIRE(state_machine.template is_state_active<State0>());
    
    state_machine.process_event(EnterSubmachine{});
    BOOST_REQUIRE(state_machine.template is_state_active<Substate0>());
    // Push to submachine's event pool explicitly to test clearance.
    submachine.process_event(DeferredEvent{});
    BOOST_REQUIRE(submachine.get_pending_events().size() == 1);
    state_machine.process_event(EnterSubstate1{});
    BOOST_REQUIRE(state_machine.template is_state_active<Substate1>());

    // Re-entry with history
    state_machine.process_event(ExitSubmachine{});
    BOOST_REQUIRE(state_machine.template is_state_active<State0>());
    state_machine.process_event(EnterSubmachine{});
    BOOST_REQUIRE(state_machine.template is_state_active<Substate1>());
    BOOST_REQUIRE(submachine.get_pending_events().size() == 1);
}

using history = shallow_history<EnterSubmachine>;
using TestMachinesShallowHistory = boost::mpl::vector<
    hierarchical_state_machine<history>,
    hierarchical_state_machine<history, favor_compile_time_config>
    >;


BOOST_AUTO_TEST_CASE_TEMPLATE(shallow_history, TestMachine, TestMachinesShallowHistory)
{
    using StateMachine = typename TestMachine::UpperMachine;
    using Submachine = typename TestMachine::Submachine;
    StateMachine state_machine;
    auto& submachine = state_machine.template get_state<Submachine>();

    state_machine.start();
    BOOST_REQUIRE(state_machine.template is_state_active<State0>());
    
    state_machine.process_event(EnterSubmachine{});
    BOOST_REQUIRE(state_machine.template is_state_active<Substate0>());
    // Push to submachine's event pool explicitly to test clearance.
    submachine.process_event(DeferredEvent{});
    BOOST_REQUIRE(submachine.get_pending_events().size() == 1);
    state_machine.process_event(EnterSubstate1{});
    BOOST_REQUIRE(state_machine.template is_state_active<Substate1>());

    // Re-entry with history
    state_machine.process_event(ExitSubmachine{});
    BOOST_REQUIRE(state_machine.template is_state_active<State0>());
    state_machine.process_event(EnterSubmachine{});
    BOOST_REQUIRE(state_machine.template is_state_active<Substate1>());
    BOOST_REQUIRE(submachine.get_pending_events().size() == 1);

    // Re-entry without history
    state_machine.process_event(ExitSubmachine{});
    BOOST_REQUIRE(state_machine.template is_state_active<State0>());
    state_machine.process_event(EnterSubmachineWithoutHistory{});
    BOOST_REQUIRE(state_machine.template is_state_active<Substate0>());
    BOOST_REQUIRE(submachine.get_pending_events().size() == 0);
}

} // namespace

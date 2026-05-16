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
#define BOOST_TEST_MODULE backmp11_copy_move_test
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
struct EnterSubmachine{};

// States.
struct MyState : public test::StateBase
{
    MyState& operator=(const MyState&)
    {
        copied_to_counter += 1;
        return *this;
    }

    MyState& operator=(MyState&&)
    {
        moved_to_counter += 1;
        return *this;
    }

    size_t copied_to_counter{};
    size_t moved_to_counter{};
};

template <typename Config = default_state_machine_config>
struct hierarchical_state_machine
{

struct StateMachine_;
struct ConfigWithRootSm;
using StateMachine = state_machine<StateMachine_, ConfigWithRootSm>;

struct ConfigWithRootSm : Config
{
    using root_sm = StateMachine;
};

struct Submachine_ : public test::StateMachineBase_<Submachine_>
{
    using initial_state = MyState;
};
using Submachine = state_machine<Submachine_, ConfigWithRootSm>;

struct StateMachine_ : public test::StateMachineBase_<StateMachine_>
{
    using initial_state = MyState;
    using transition_table = mp11::mp_list<
        Row<MyState, EnterSubmachine, Submachine>
    >;
};

};

using TestMachines = mp11::mp_list<
    hierarchical_state_machine<>,
    hierarchical_state_machine<favor_compile_time_config>
    >;

BOOST_AUTO_TEST_CASE_TEMPLATE(copy_operators, StateMachine, TestMachines)
{
    using TestMachine = typename StateMachine::StateMachine;
    TestMachine test_machine;

    test_machine.start();

    test_machine.process_event(EnterSubmachine{});
    BOOST_REQUIRE(test_machine.template is_state_active<typename StateMachine::Submachine>());

// MSVC uses the variadic arg constructor instead of the copy constructor
// (which shouldn't even compile because the argument cannot be forwarded to the front-end).
#ifndef BOOST_MSVC
    {
        TestMachine other_test_machine{test_machine};
        auto& other_submachine = other_test_machine.template get_state<typename StateMachine::Submachine>();
        BOOST_REQUIRE(other_test_machine.template is_state_active<typename StateMachine::Submachine>());
        BOOST_REQUIRE(&test_machine.get_root_sm() != &other_test_machine.get_root_sm());

        auto& other_submachine_state = other_submachine.template get_state<MyState>();
        ASSERT_ONE_AND_RESET(other_submachine_state.copied_to_counter);
    }
#endif

    {
        TestMachine other_test_machine;
        other_test_machine = test_machine;
        auto& other_submachine = other_test_machine.template get_state<typename StateMachine::Submachine>();
        BOOST_REQUIRE(other_test_machine.template is_state_active<typename StateMachine::Submachine>());
        BOOST_REQUIRE(&test_machine.get_root_sm() != &other_test_machine.get_root_sm());

        auto& other_submachine_state = other_submachine.template get_state<MyState>();
        ASSERT_ONE_AND_RESET(other_submachine_state.copied_to_counter);
    }

    test_machine.stop();
}

BOOST_AUTO_TEST_CASE_TEMPLATE(move_operators, StateMachine, TestMachines)
{
    using TestMachine = typename StateMachine::StateMachine;
    TestMachine test_machine;

    test_machine.start();

    test_machine.process_event(EnterSubmachine{});
    BOOST_REQUIRE(test_machine.template is_state_active<typename StateMachine::Submachine>());

    {
        TestMachine other_test_machine{std::move(test_machine)};
        auto& other_submachine = other_test_machine.template get_state<typename StateMachine::Submachine>();
        BOOST_REQUIRE(other_test_machine.template is_state_active<typename StateMachine::Submachine>());
        BOOST_REQUIRE(&test_machine.get_root_sm() != &other_test_machine.get_root_sm());

        auto& other_submachine_state = other_submachine.template get_state<MyState>();
        ASSERT_ONE_AND_RESET(other_submachine_state.moved_to_counter);
    }

    {
        TestMachine other_test_machine;
        other_test_machine = std::move(test_machine);
        auto& other_submachine = other_test_machine.template get_state<typename StateMachine::Submachine>();
        BOOST_REQUIRE(other_test_machine.template is_state_active<typename StateMachine::Submachine>());
        BOOST_REQUIRE(&test_machine.get_root_sm() != &other_test_machine.get_root_sm());

        auto& other_submachine_state = other_submachine.template get_state<MyState>();
        ASSERT_ONE_AND_RESET(other_submachine_state.moved_to_counter);
    }

    test_machine.stop();
}

BOOST_AUTO_TEST_CASE_TEMPLATE(copy_event_pool, StateMachine, TestMachines)
{
    using TestMachine = typename StateMachine::StateMachine;
    TestMachine test_machine;

    test_machine.start();

    test_machine.enqueue_event(EnterSubmachine{});

    {
        TestMachine other_test_machine;
        other_test_machine = test_machine;
        BOOST_REQUIRE(other_test_machine.process_event_pool() == 1);
        BOOST_REQUIRE(other_test_machine.template is_state_active<typename StateMachine::Submachine>());
    }

    BOOST_REQUIRE(test_machine.process_event_pool() == 1);
    BOOST_REQUIRE(test_machine.template is_state_active<typename StateMachine::Submachine>());

    test_machine.stop();
}

BOOST_AUTO_TEST_CASE_TEMPLATE(move_event_pool, StateMachine, TestMachines)
{
    using TestMachine = typename StateMachine::StateMachine;
    TestMachine test_machine;

    test_machine.start();

    test_machine.enqueue_event(EnterSubmachine{});

    {
        TestMachine other_test_machine{std::move(test_machine)};
        BOOST_REQUIRE(other_test_machine.process_event_pool() == 1);
        BOOST_REQUIRE(other_test_machine.template is_state_active<typename StateMachine::Submachine>());
    }

    BOOST_REQUIRE(test_machine.process_event_pool() == 0);
    BOOST_REQUIRE(test_machine.template is_state_active<MyState>());

    test_machine.stop();
}

} // namespace

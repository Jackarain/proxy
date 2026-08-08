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
#define BOOST_TEST_MODULE backmp11_exceptions_test
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

enum class throw_time
{
    machine_entry,
    machine_exit,
    completion_event,
    transition_guard,
    source_exit_action,
    transition_action,
    target_entry_action
};

// Events.
struct TriggerException
{
    throw_time throw_if;
};
struct TriggerCompletionTransition {};

// Actions
struct MyAction
{
    template<typename Fsm>
    void operator()(const TriggerException& event, Fsm& fsm)
    {
        fsm.action_calls++;
        if (event.throw_if == throw_time::transition_action)
        {
            throw std::exception{};
        }
    }
};

// Guards
struct MyGuard
{
    template<typename Fsm>
    bool operator()(const TriggerException& event, Fsm& fsm) const
    {
        fsm.guard_calls++;
        if (event.throw_if == throw_time::transition_guard)
        {
            throw std::exception{};
        }
        return true;
    }
};

// States.
struct MyState : test::StateBase
{
    template <typename Event, typename Fsm>
    void on_entry(const Event&, Fsm&)
    {
    }

    template <typename Fsm>
    void on_entry(const TriggerException& event, Fsm&)
    {
        on_entry_calls++;
        if (event.throw_if == throw_time::target_entry_action)
        {
            throw std::exception{};
        }
    }

    template <typename Fsm>
    void on_exit(const TriggerException& event, Fsm&)
    {
        on_exit_calls++;
        if (event.throw_if == throw_time::source_exit_action)
        {
            throw std::exception{};
        }
    }

    template <typename Event, typename Fsm>
    void on_exit(const Event&, Fsm&)
    {
    }

    size_t on_entry_calls{};
    size_t on_exit_calls{};
};
struct MyOtherState : MyState {};
struct ShortState : test::StateBase {};
struct AlwaysThrowingState : test::StateBase
{
    template <typename Event, typename Fsm>
    void on_entry(const Event&, Fsm&)
    {
        throw std::exception{};
    }
};

struct CompositeState : public test::StateMachineBase_<CompositeState>
{
    using initial_state = MyState;
    using transition_table = mp11::mp_list<
        // First row must not have MyState to give it a state id other than 0 for this test.
        Row<ShortState  , none                       , AlwaysThrowingState, none    , none>,
        Row<MyState     , TriggerCompletionTransition, ShortState         , none    , none>,
        Row<MyState     , TriggerException           , MyOtherState       , MyAction, MyGuard>,
        Row<MyOtherState, TriggerException           , MyState            , MyAction, MyGuard>
    >;

    template <typename Fsm>
    void on_entry(const TriggerException& event, Fsm&)
    {
        if (event.throw_if == throw_time::machine_entry)
        {
            throw std::exception{};
        }
    }

    template <typename Event, typename Fsm>
    void on_entry(const Event&, Fsm&)
    {
    }

    template <typename Fsm>
    void on_exit(const TriggerException& event, Fsm&)
    {
        if (event.throw_if == throw_time::machine_exit)
        {
            throw std::exception{};
        }
    }

    template <typename Event, typename Fsm>
    void on_exit(const Event&, Fsm&)
    {
    }

    size_t guard_calls{};
    size_t action_calls{};
};

template <typename StateMachine>
bool try_catch_process_event_exception(StateMachine& sm, throw_time throw_if)
{
    bool result{};
    try
    {
        sm.process_event(TriggerException{throw_if});
    }
    catch (const std::exception&)
    {
        result = true;
    }
    return result;
}

template <typename F>
bool try_catch_exception(F&& f)
{
    bool result{};
    try
    {
        f();
    }
    catch (const std::exception&)
    {
        result = true;
    }
    return result;
}

template <typename Config = state_machine_config>
using StateMachine = state_machine<CompositeState, Config>;

using TestMachines = mp11::mp_list<
    StateMachine<>,
    StateMachine<favor_compile_time_config>
    >;

BOOST_AUTO_TEST_CASE_TEMPLATE(start_stop, TestMachine, TestMachines)
{
    TestMachine test_machine;

    // Exception during start:
    // The state machine becomes active and sets the initial state.
    static_assert(test_machine.template get_state_id<MyState>() != 0);
    BOOST_REQUIRE(try_catch_exception([&test_machine] {
        test_machine.start(TriggerException{throw_time::machine_entry});
    }));
    BOOST_REQUIRE(test_machine.template is_state_active<MyState>());

    // Exception during stop:
    // The state machine remains active.
    BOOST_REQUIRE(try_catch_exception([&test_machine] {
        test_machine.stop(TriggerException{throw_time::machine_exit});
    }));
    BOOST_REQUIRE(test_machine.template is_state_active<MyState>());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(external_transition, TestMachine, TestMachines)
{
    TestMachine test_machine;
    MyState& my_state = test_machine.template get_state<MyState>();
    CompositeState& composite_state = test_machine;
    MyOtherState& my_other_state = test_machine.template get_state<MyOtherState>();

    test_machine.start();

    BOOST_REQUIRE(try_catch_process_event_exception(
        test_machine, throw_time::transition_guard));
    BOOST_REQUIRE(test_machine.template is_state_active<MyState>());
    ASSERT_AND_RESET(composite_state.guard_calls, 1);
    ASSERT_ZERO(my_state.on_exit_calls);

    BOOST_REQUIRE(try_catch_process_event_exception(test_machine, throw_time::source_exit_action));
    BOOST_REQUIRE(test_machine.template is_state_active<MyState>());
    ASSERT_AND_RESET(my_state.on_exit_calls, 1);
    ASSERT_ZERO(composite_state.action_calls);

    BOOST_REQUIRE(try_catch_process_event_exception(test_machine, throw_time::transition_action));
    BOOST_REQUIRE(test_machine.template is_state_active<MyOtherState>());
    ASSERT_AND_RESET(composite_state.action_calls, 1);
    ASSERT_ZERO(my_other_state.on_entry_calls);

    BOOST_REQUIRE(try_catch_process_event_exception(test_machine, throw_time::target_entry_action));
    BOOST_REQUIRE(test_machine.template is_state_active<MyState>());
    ASSERT_AND_RESET(my_state.on_entry_calls, 1);

    test_machine.stop();
}

BOOST_AUTO_TEST_CASE_TEMPLATE(completion_transition, TestMachine, TestMachines)
{
    TestMachine test_machine;

    test_machine.start();

    BOOST_REQUIRE(try_catch_exception([&test_machine] {
        test_machine.process_event(TriggerCompletionTransition{});
    }));
    BOOST_REQUIRE(test_machine.template is_state_active<AlwaysThrowingState>());

    test_machine.stop();
}

} // namespace

// Copyright 2026 Christian Granzin
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
#define BOOST_TEST_MODULE backmp11_observer_test
#endif
#include <boost/test/unit_test.hpp>

#include "Backmp11.hpp"
#include <boost/msm/backmp11/observer.hpp>

#include "FrontCommon.hpp"
#include "Utils.hpp"

#include "attachments/backmp11/InterruptibleStateMachine.cpp"

namespace msm = boost::msm;
namespace mp11 = boost::mp11;

using namespace msm::front;
using namespace msm::backmp11;

namespace
{

// Events.
struct TerminateEvent {};

// States.
using MyTerminateState = terminate_state<>;

// State machine.
struct TestMachine_ : state_machine_def<TestMachine_>
{
    using initial_state = MyState;
    using transition_table = mp11::mp_list<
        Row<MyState         , TerminateEvent, MyTerminateState>,
        Row<MyTerminateState, TerminateEvent, MyState>
    >;
    using internal_transition_table = mp11::mp_list<
        Internal<SmInternalEvent>
    >;
};

struct TestConfig : state_machine_config
{
    using observer = TestObserver;
};

struct TestFavorCompileTimeConfig : TestConfig
{
    using compile_policy = favor_compile_time;
};

template <typename Config = TestConfig>
class TestMachine
    : public state_machine<TestMachine_, Config, TestMachine<Config>>
{
    using Base = state_machine<TestMachine_, Config, TestMachine<Config>>;

  public:
    using Base::Base;

    void interrupt()
    {
        m_interrupted = true;
    }

    void resume()
    {
        m_interrupted = false;
        this->process_event_pool();
    }

    template <typename Event>
    process_result process_event(const Event& event)
    {
        if (m_interrupted)
        {
            // Enqueue all events for later processing
            // as long as the state machine is interrupted.
            this->enqueue_event(event);
            return process_result::deferred;
        }
        else
        {
            return Base::process_event(event);
        }
    }

  private:
    bool m_interrupted{};
};

using TestMachines = mp11::mp_list<
    TestMachine<>,
    TestMachine<TestFavorCompileTimeConfig>
    >;


BOOST_AUTO_TEST_CASE_TEMPLATE(start_stop, StateMachine, TestMachines)
{
    StateMachine test_machine;
    
    BOOST_REQUIRE(test_machine.get_machine_state() == machine_state::stopped);
    test_machine.start();
    BOOST_REQUIRE(test_machine.get_machine_state() == machine_state::idle);
    test_machine.stop();
    BOOST_REQUIRE(test_machine.get_machine_state() == machine_state::stopped);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(terminate, StateMachine, TestMachines)
{
    StateMachine test_machine;
    TestObserver& observer = test_machine.get_observer();
    
    test_machine.start();
    BOOST_REQUIRE(test_machine.process_event(TerminateEvent{}) ==
                  process_result::consumed);
    ASSERT_ONE_AND_RESET(observer.processed_events);

    observer.expect_transition = false;
    BOOST_REQUIRE(test_machine.process_event(SmInternalEvent{}) ==
                  process_result::discarded);
    ASSERT_ONE_AND_RESET(observer.processed_events);
}

using InterruptibleTestMachines = mp11::mp_list<
    InterruptibleStateMachine<TestConfig>,
    InterruptibleStateMachine<TestFavorCompileTimeConfig>
    >;

BOOST_AUTO_TEST_CASE_TEMPLATE(interrupt, StateMachine, InterruptibleTestMachines)
{
    StateMachine test_machine;
    TestObserver& observer = test_machine.get_observer();
    PrintMessage::talk = false;
    
    test_machine.start();
    BOOST_REQUIRE(test_machine.process_event(InterruptEvent{}) ==
                  process_result::consumed);
    ASSERT_ONE_AND_RESET(observer.processed_events);

    BOOST_REQUIRE(test_machine.process_event(SmInternalEvent{}) ==
                  process_result::deferred);
    // The state machine extension intercepts the event.
    ASSERT_ZERO(observer.processed_events);

    observer.expect_transition = true;
    observer.expect_process_result = process_result::consumed;
    test_machine.resume();
    ASSERT_ONE_AND_RESET(observer.processed_events);
}

} // namespace

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

#include <boost/msm/backmp11/state_machine.hpp>
#include <boost/msm/backmp11/favor_compile_time.hpp>

#include "FrontCommon.hpp"
#include "Utils.hpp"

#include "attachments/backmp11/Logger.cpp"

namespace msm = boost::msm;
namespace mp11 = boost::mp11;

using namespace msm::front;
using namespace msm::backmp11;

namespace
{

class Observer : public Logger
{
  public:
    template <typename StateMachine, typename Event>
    void pre_process_event(const StateMachine& sm, const Event& event)
    {
        Logger::pre_process_event(sm, event);
        pre_process_event_counter++;
    }

    template <typename StateMachine, typename Event>
    void post_process_event(const StateMachine&, const Event&, process_result /*result*/)
    {
        post_process_event_counter++;
    }

    template <typename Source, typename Event, typename Target, typename Action,
              typename Guard, typename StateMachine>
    void pre_process_transition(const StateMachine&, size_t /*region_id*/)
    {
        pre_process_transition_counter++;
    }

    template <typename Source, typename Event, typename Target, typename Action,
              typename Guard, typename StateMachine>
    void post_process_transition(const StateMachine& sm, size_t region_id,
                                 process_result result)
    {
        Logger::post_process_transition<Source, Event, Target, Action, Guard>(
            sm, region_id, result);
        post_process_transition_counter++;
    }

    template <typename Event, typename Action, typename Guard,
              typename StateMachine>
    void pre_process_transition(const StateMachine&)
    {
        pre_process_transition_counter++;
    }

    template <typename Event, typename Action, typename Guard,
              typename StateMachine>
    void post_process_transition(const StateMachine& sm, process_result result)
    {
        Logger::post_process_transition<Event, Action, Guard>(sm, result);
        post_process_transition_counter++;
    }

    void log(std::string_view msg) override
    {
        messages.push_back(std::string{msg});
    }

    void assert_and_pop_msg(std::string_view msg)
    {
        BOOST_REQUIRE(messages.front() == msg);
        messages.pop_front();
    }

    std::deque<std::string> messages;
    size_t pre_start_counter{};
    size_t post_start_counter{};
    size_t pre_stop_counter{};
    size_t post_stop_counter{};    
    size_t pre_process_event_counter{};
    size_t post_process_event_counter{};
    size_t pre_process_transition_counter{};
    size_t post_process_transition_counter{};
};

namespace observer_test
{

struct default_config : state_machine_config
{
    using observer = Observer;
};
struct favor_compile_time_config : default_config
{
    using compile_policy = favor_compile_time;
};

template <typename Config = default_config>
using SmWithObserver = state_machine<MyStateMachine, Config>;

using TestMachines = mp11::mp_list<
    SmWithObserver<>,
    SmWithObserver<favor_compile_time_config>
    >;


BOOST_AUTO_TEST_CASE_TEMPLATE(observer, StateMachine, TestMachines)
{
    StateMachine test_machine;
    Observer& observer = test_machine.get_observer();

    test_machine.start();
    observer.assert_and_pop_msg("MyStateMachine processed transition \"none + starting -> MyState\" (consumed)");
    ASSERT_AND_RESET(observer.pre_process_transition_counter, 1);
    ASSERT_AND_RESET(observer.post_process_transition_counter, 1);

    test_machine.process_event(TransitionEvent{});
    observer.assert_and_pop_msg("MyStateMachine processing event TransitionEvent");
    observer.assert_and_pop_msg("MyStateMachine processed transition \"MyState + TransitionEvent [ NotMyGuard ] / MyAction -> MyOtherState\" (rejected)");
    observer.assert_and_pop_msg("MyStateMachine processed transition \"MyState + TransitionEvent [ MyGuard ] / MyAction -> MyOtherState\" (consumed)");
    ASSERT_AND_RESET(observer.pre_process_event_counter, 1);
    ASSERT_AND_RESET(observer.pre_process_transition_counter, 2);
    ASSERT_AND_RESET(observer.post_process_transition_counter, 2);
    ASSERT_AND_RESET(observer.post_process_event_counter, 1);

    test_machine.process_event(TransitionEvent{});
    observer.assert_and_pop_msg("MyStateMachine processing event TransitionEvent");
    observer.assert_and_pop_msg("MyStateMachine processed transition \"MyOtherState + TransitionEvent -> MyState\" (consumed)");
    ASSERT_AND_RESET(observer.pre_process_event_counter, 1);
    ASSERT_AND_RESET(observer.pre_process_transition_counter, 1);
    ASSERT_AND_RESET(observer.post_process_transition_counter, 1);
    ASSERT_AND_RESET(observer.post_process_event_counter, 1);

    test_machine.process_event(InternalTransitionEvent{});
    observer.assert_and_pop_msg("MyStateMachine processing event InternalTransitionEvent");
    observer.assert_and_pop_msg("MyStateMachine processed transition \"MyState + InternalTransitionEvent / MyAction\" (consumed)");
    ASSERT_AND_RESET(observer.pre_process_event_counter, 1);
    ASSERT_AND_RESET(observer.pre_process_transition_counter, 1);
    ASSERT_AND_RESET(observer.post_process_transition_counter, 1);
    ASSERT_AND_RESET(observer.post_process_event_counter, 1);

    test_machine.process_event(SmInternalTransitionEvent{});
    observer.assert_and_pop_msg("MyStateMachine processing event SmInternalTransitionEvent");
    observer.assert_and_pop_msg("MyStateMachine processed transition \"MyStateMachine + SmInternalTransitionEvent / MyAction\" (consumed)");
    ASSERT_AND_RESET(observer.pre_process_event_counter, 1);
    ASSERT_AND_RESET(observer.pre_process_transition_counter, 1);
    ASSERT_AND_RESET(observer.post_process_transition_counter, 1);
    ASSERT_AND_RESET(observer.post_process_event_counter, 1);

    test_machine.stop();
}

} // namespace observer

namespace observer_ref_test
{

struct default_config : state_machine_config
{
    using observer = observer_ref<Observer>;
};
struct favor_compile_time_config : default_config
{
    using compile_policy = favor_compile_time;
};

template <typename Config = default_config>
using SmWithObserver = state_machine<MyStateMachine, Config>;

using TestMachines = mp11::mp_list<
    SmWithObserver<>,
    SmWithObserver<favor_compile_time_config>
    >;


BOOST_AUTO_TEST_CASE_TEMPLATE(observer_ref, StateMachine, TestMachines)
{
    Observer observer;
    StateMachine test_machine{observer};

    test_machine.start();
    observer.assert_and_pop_msg("MyStateMachine processed transition \"none + starting -> MyState\" (consumed)");
    ASSERT_AND_RESET(observer.pre_process_transition_counter, 1);
    ASSERT_AND_RESET(observer.post_process_transition_counter, 1);

    test_machine.process_event(TransitionEvent{});
    observer.assert_and_pop_msg("MyStateMachine processing event TransitionEvent");
    observer.assert_and_pop_msg("MyStateMachine processed transition \"MyState + TransitionEvent [ NotMyGuard ] / MyAction -> MyOtherState\" (rejected)");
    observer.assert_and_pop_msg("MyStateMachine processed transition \"MyState + TransitionEvent [ MyGuard ] / MyAction -> MyOtherState\" (consumed)");
    ASSERT_AND_RESET(observer.pre_process_event_counter, 1);
    ASSERT_AND_RESET(observer.pre_process_transition_counter, 2);
    ASSERT_AND_RESET(observer.post_process_transition_counter, 2);
    ASSERT_AND_RESET(observer.post_process_event_counter, 1);

    test_machine.process_event(TransitionEvent{});
    observer.assert_and_pop_msg("MyStateMachine processing event TransitionEvent");
    observer.assert_and_pop_msg("MyStateMachine processed transition \"MyOtherState + TransitionEvent -> MyState\" (consumed)");
    ASSERT_AND_RESET(observer.pre_process_event_counter, 1);
    ASSERT_AND_RESET(observer.pre_process_transition_counter, 1);
    ASSERT_AND_RESET(observer.post_process_transition_counter, 1);
    ASSERT_AND_RESET(observer.post_process_event_counter, 1);

    test_machine.process_event(InternalTransitionEvent{});
    observer.assert_and_pop_msg("MyStateMachine processing event InternalTransitionEvent");
    observer.assert_and_pop_msg("MyStateMachine processed transition \"MyState + InternalTransitionEvent / MyAction\" (consumed)");
    ASSERT_AND_RESET(observer.pre_process_event_counter, 1);
    ASSERT_AND_RESET(observer.pre_process_transition_counter, 1);
    ASSERT_AND_RESET(observer.post_process_transition_counter, 1);
    ASSERT_AND_RESET(observer.post_process_event_counter, 1);

    test_machine.process_event(SmInternalTransitionEvent{});
    observer.assert_and_pop_msg("MyStateMachine processing event SmInternalTransitionEvent");
    observer.assert_and_pop_msg("MyStateMachine processed transition \"MyStateMachine + SmInternalTransitionEvent / MyAction\" (consumed)");
    ASSERT_AND_RESET(observer.pre_process_event_counter, 1);
    ASSERT_AND_RESET(observer.pre_process_transition_counter, 1);
    ASSERT_AND_RESET(observer.post_process_transition_counter, 1);
    ASSERT_AND_RESET(observer.post_process_event_counter, 1);

    test_machine.stop();
}

}

} // namespace

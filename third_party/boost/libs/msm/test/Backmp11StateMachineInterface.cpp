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
#define BOOST_TEST_MODULE test_heapless_state_machine
#endif
#include <boost/test/unit_test.hpp>

// back-end
#include "BackCommon.hpp"
#include <boost/msm/backmp11/observer.hpp>
//front-end
#include "FrontCommon.hpp"

#include "Utils.hpp"
#include "attachments/backmp11/StateMachineInterface.cpp"

namespace msm = boost::msm;

using namespace msm::front;
using namespace msm::backmp11;

namespace
{

struct EventObserver : default_observer
{
    template <typename StateMachine>
    void pre_process_event(const StateMachine&, const std::any&)
    {
        processed_greet_events++;
    }

    size_t processed_greet_events{};
};

struct TestConfig : MyConfig
{
    using observer = EventObserver;
};

using TestMachine = state_machine_impl<MyStateMachine_, TestConfig>;
using OtherTestMachine = state_machine_impl<MyOtherStateMachine_, TestConfig>;

BOOST_AUTO_TEST_CASE(state_machine_interface)
{
    PrintMessage::talk = false;

    std::unique_ptr<state_machine> state_machine;
    
    state_machine = std::make_unique<TestMachine>();
    EventObserver* observer =
        &dynamic_cast<TestMachine&>(*state_machine).get_observer();
    state_machine->start();
    BOOST_REQUIRE(state_machine->is_state_active("MyState"));
    state_machine->process_event(Greet{});
    BOOST_REQUIRE(observer->processed_greet_events == 1);

    state_machine = std::make_unique<OtherTestMachine>();
    observer = &dynamic_cast<OtherTestMachine&>(*state_machine).get_observer();
    state_machine->start();
    BOOST_REQUIRE(state_machine->is_state_active("MyOtherState"));
    state_machine->process_event(Greet{});
    BOOST_REQUIRE(observer->processed_greet_events == 1);
}

} // namespace

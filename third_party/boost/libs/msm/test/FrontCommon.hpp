// Copyright 2026 Christian Granzin
// Copyright 2024 Christophe Henry
// henry UNDERSCORE christophe AT hotmail DOT com
// This is an extended version of the state machine available in the boost::mpl library
// Distributed under the same license as the original.
// Copyright for the original version:
// Copyright 2005 David Abrahams and Aleksey Gurtovoy. Distributed
// under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/test/tools/interface.hpp>

#include <boost/msm/front/state_machine_def.hpp>
#include <boost/msm/front/functor_row.hpp>

namespace boost { namespace msm { namespace front
{
namespace test
{

// Base for a state used in the tests.
struct StateBase : state<>
{
    template <class Event, class Fsm>
    void on_entry(const Event&, Fsm&)
    {
        entry_counter++;
    }
    
    template <class Event, class Fsm>
    void on_exit(const Event&, Fsm&)
    {
        exit_counter++;
    }

    size_t entry_counter{};
    size_t exit_counter{};
    size_t action_counter{};
};

// Base for a state machine used in the tests.
template <typename T>
struct StateMachineBase_ : state_machine_def<T>
{
    template <class Event, class Fsm>
    void on_entry(const Event&, Fsm&)
    {
        entry_counter++;
    }
    
    template <class Event, class Fsm>
    void on_exit(const Event&, Fsm&)
    {
        exit_counter++;
    }

    template <class FSM, class Event>
    void no_transition(Event const&, FSM&, int)
    {
        BOOST_TEST_FAIL("no_transition called!");
    }

    size_t entry_counter{};
    size_t exit_counter{};
    size_t action_counter{};
};


} // namespace test
}}} // namespace boost::msm::front

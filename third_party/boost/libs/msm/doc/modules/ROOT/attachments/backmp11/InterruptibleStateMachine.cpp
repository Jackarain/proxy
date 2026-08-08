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

#include <iostream>

#include <boost/msm/backmp11/state_machine.hpp>
#include <boost/msm/front/functor_row.hpp>
#include <boost/msm/front/state_machine_def.hpp>

namespace back = boost::msm::backmp11;
namespace front = boost::msm::front;
using front::Row;
using front::Internal;
namespace mp11 = boost::mp11;

namespace
{

// Events
struct InterruptEvent {};
struct SmInternalEvent {};

// Actions
struct Interrupt
{
    template <typename Fsm>
    void operator()(Fsm& fsm)
    {
        fsm.interrupt();
    }
};

struct PrintMessage
{
    template <typename Fsm>
    void operator()(const SmInternalEvent&, Fsm&)
    {
        if (talk)
        {
            std::cout << "Processed SmInternalEvent" << std::endl;
        }
    }

    [[maybe_unused]] static inline bool talk{true};
};

// States
struct MyState : front::state<> {};

struct MyOtherState : front::state<> {};

// State machine
struct InterruptibleStateMachine_
    : front::state_machine_def<InterruptibleStateMachine_>
{
    using initial_state = MyState;
    using transition_table = mp11::mp_list<
        Row<MyState, InterruptEvent, front::none, Interrupt>
    >;
    using internal_transition_table = mp11::mp_list<
        Internal<SmInternalEvent, PrintMessage>
    >;
};

template <typename Config = back::state_machine_config>
class InterruptibleStateMachine
    : public back::state_machine<InterruptibleStateMachine_, Config,
                                 InterruptibleStateMachine<Config>>
{
    using Base = back::state_machine<InterruptibleStateMachine_, Config,
                                     InterruptibleStateMachine<Config>>;

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
    back::process_result process_event(const Event& event)
    {
        if (m_interrupted)
        {
            // Enqueue all events for later processing
            // as long as the state machine is interrupted.
            this->enqueue_event(event);
            return back::process_result::deferred;
        }
        else
        {
            return Base::process_event(event);
        }
    }

  private:
    bool m_interrupted{};
};


[[maybe_unused]] void interruptible_state_machine_example()
{
    InterruptibleStateMachine<> state_machine;
    
    state_machine.start();

    // Interrupt the state machine.
    state_machine.process_event(InterruptEvent{});

    // The event is not yet processed.
    state_machine.process_event(SmInternalEvent{});

    // Resuming the state machine processes the event and prints:
    // "Processed SmInternalEvent"
    state_machine.resume();
}

} // namespace


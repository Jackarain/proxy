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

#include <boost/container/static_vector.hpp>

#include <boost/msm/backmp11/state_machine.hpp>
#include <boost/msm/front/functor_row.hpp>
#include <boost/msm/front/state_machine_def.hpp>

namespace back = boost::msm::backmp11;
namespace front = boost::msm::front;
using front::none;
using front::Row;
namespace mp11 = boost::mp11;

namespace
{

static constexpr size_t max_event_size = 32;

struct MyConfig : back::state_machine_config
{
    // Use a static vector as event pool container.
    // This container is sufficient for enqueueing and deferring events,
    // but it does not support completion transitions.
    // If needed, use a heapless deque implementation (e.g. etl::deque).
    template <typename T>
    using static_vector = boost::container::static_vector<T, 10>;
    using event_pool =
        back::event_pool</*Container=*/static_vector,
                         /*InlineCapacity=*/max_event_size>;
};

// Events
struct Greet
{
    // A heapless event must be copy constructible and nothrow move constructible.
    std::array<char, max_event_size> message{};
};

// Actions
struct PrintMessage
{
    template <typename Fsm>
    void operator()(const Greet& greet, Fsm&)
    {
        std::cout << greet.message.data();
    }
};

// States
struct MyState : front::state<> {};

// State machine
struct MyStateMachine_ : front::state_machine_def<MyStateMachine_>
{
    template <typename Fsm>
    void on_entry(const back::starting&, Fsm& fsm)
    {
        fsm.enqueue_event(Greet{"Hello"});
        fsm.enqueue_event(Greet{" heapless"});
        fsm.enqueue_event(Greet{" world\n"});
    }

    using initial_state = MyState;
    using transition_table = mp11::mp_list<
        Row<MyState, Greet, none, PrintMessage>
    >;
};

using MyStateMachine = back::state_machine<MyStateMachine_, MyConfig>;

[[maybe_unused]] void heapless_state_machine_example()
{
    MyStateMachine state_machine;

    // Prints "Hello heapless world",
    // triggered by multiple events processed from the event pool.
    state_machine.start();
}

}

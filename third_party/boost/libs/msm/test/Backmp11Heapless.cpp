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
//front-end
#include "FrontCommon.hpp"

#include "Utils.hpp"
#include "attachments/backmp11/HeaplessStateMachine.cpp"

namespace msm = boost::msm;

using namespace msm::front;
using namespace msm::backmp11;

static bool g_heap_forbidden = false;

void* operator new(std::size_t size)
{
    if (g_heap_forbidden)
        throw std::bad_alloc{};
    void* p = std::malloc(size);
    if (!p)
        throw std::bad_alloc{};
    return p;
}

namespace
{

struct CountEvent
{
    template <typename Fsm>
    void operator()(const Greet&, Fsm& fsm)
    {
        fsm.events++;
    }
};

// State machine.
struct TestMachine_ : front::state_machine_def<MyStateMachine_>
{
    template <typename Fsm>
    void on_entry(const back::starting&, Fsm& fsm)
    {
        fsm.enqueue_event(Greet{});
        fsm.enqueue_event(Greet{});
        fsm.enqueue_event(Greet{});
    }

    using initial_state = MyState;
    using transition_table = mp11::mp_list<
        Row<MyState, Greet, none, CountEvent>
    >;

    size_t events{};
};

using TestMachine = state_machine<TestMachine_, MyConfig>;

BOOST_AUTO_TEST_CASE(heapless_state_machine)
{
    using polymorphic_t = msm::backmp11::detail::basic_polymorphic<
        msm::backmp11::detail::event_occurrence>;
    
    TestMachine* sm_ptr{nullptr};
    using deferred_t = msm::backmp11::detail::deferred_event<Greet>;
    static_assert(std::is_nothrow_move_constructible_v<deferred_t>);
    const auto deferred_event =
        deferred_t{*sm_ptr, Greet{}, 0};
    const auto event_occurrence = polymorphic_t::make(deferred_event);
    BOOST_REQUIRE_MESSAGE(event_occurrence.is_inline(),
                     "Event must fit in inline storage");

    g_heap_forbidden = true;

    TestMachine sm;

    sm.start();
    ASSERT_AND_RESET(sm.events, 3);

    g_heap_forbidden = false;
}

} // namespace

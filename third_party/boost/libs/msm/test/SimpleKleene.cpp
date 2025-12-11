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

// back-end
#include "BackCommon.hpp"
// front-end
#include <boost/msm/front/state_machine_def.hpp>
#include <boost/msm/front/functor_row.hpp>

#ifndef BOOST_MSM_NONSTANDALONE_TEST
#define BOOST_TEST_MODULE simple_kleene_test
#endif
#include <boost/test/unit_test.hpp>

namespace msm = boost::msm;
namespace front = msm::front;
using front::Row;
using front::state_machine_def;
namespace mp11 = boost::mp11;

namespace {

template <bool Value>
struct Guard
{
    template <typename Event, typename Fsm, typename SourceState, typename TargetState>
    bool operator()(const Event&, Fsm&, SourceState&, TargetState&)
    {
        return Value;
    }
};

template <int E>
struct Event {};

template <int I>
struct Action
{
    template <typename Event, typename Fsm, typename SourceState, typename TargetState>
    void operator()(const Event&, Fsm&, SourceState&, TargetState&)
    {
        return;
    }
};

template <int I>
struct State : front::state<>
{
    template <class Event,class Fsm>
    void on_entry(Event const&, Fsm&)
    {
        active = true;
    }

    template <class Event,class Fsm>
    void on_exit(Event const&, Fsm&)
    {
        active = false;
    }

    static constexpr int value = I;

    bool active = false;
};

struct Simple_ : front::state_machine_def<Simple_> 
{
    using transition_table = mp11::mp_list<
        Row<State<0>, Event<0>, State<1>, Action<0>, Guard<false>>,
        Row<State<0>, boost::any, State<1>, Action<1>, Guard<true>>
    >;

    using initial_state = State<0>;
};

// Pick a back-end
using Fsms = mp11::mp_list<
#ifndef BOOST_MSM_TEST_SKIP_BACKMP11
    msm::backmp11::state_machine_adapter<Simple_>,
#endif // BOOST_MSM_TEST_SKIP_BACKMP11
    msm::back::state_machine<Simple_>
    // back11 requires a const boost::any overload to identify the Kleene event.
    // Leave it out of this test to ensure backwards compatibility.
    // msm::back11::state_machine<Front>
    >;

BOOST_AUTO_TEST_CASE_TEMPLATE(simple_kleene_test, Fsm, Fsms)
{     
    Fsm fsm;

    fsm.start();
    BOOST_CHECK_MESSAGE(fsm.template get_state<State<0>&>().active == true, "State<0> entry not called correctly");

    fsm.process_event(Event<0>{});
    BOOST_CHECK_MESSAGE(fsm.template get_state<State<0>&>().active == false, "State<0> exit not called correctly");
    BOOST_CHECK_MESSAGE(fsm.template get_state<State<1>&>().active == true, "State<1> entry not called correctly");
}

} // namespace
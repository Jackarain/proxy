// Copyright 2026 Christian Granzin
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <boost/msm/backmp11/state_machine.hpp>
#include <boost/msm/front/state_machine_def.hpp>
#include <boost/msm/front/functor_row.hpp>

#ifndef BOOST_MSM_EXAMPLE_DIM_SWITCH_HPP
#define BOOST_MSM_EXAMPLE_DIM_SWITCH_HPP

using namespace boost::msm;
namespace mp11 = boost::mp11;

namespace
{

// States.

// An empty state (doesn't need reflection).
struct Off : front::state<> {};

// A state with a reflect free function.
struct On : front::state<>
{
    template <typename Event, typename Fsm>
    void on_entry(const Event&, Fsm&)
    {
        times_pressed += 1;
    }

// ADL with MSVC does not work correctly.
#ifdef BOOST_MSVC
    template <typename Visitor>
    void reflect(Visitor&& visitor)
    {
        visitor.visit_member("times_pressed", times_pressed);
    }
    template <typename Visitor>
    void reflect(Visitor&& visitor) const
    {
        visitor.visit_member("times_pressed", times_pressed);
    }
#endif 

    uint32_t times_pressed{};
};

template <typename Visitor>
void reflect(On& on, Visitor&& visitor)
{
    visitor.visit_member("times_pressed", on.times_pressed);
}

template <typename Visitor>
void reflect(const On& on, Visitor&& visitor)
{
    visitor.visit_member("times_pressed", on.times_pressed);
}

// Events.
struct TurnOn {};

struct TurnOff {};

struct Dim
{
    uint8_t brightness;
};

// Actions.
struct SetDimValue
{
    template <typename Fsm>
    void operator()(const Dim& event, Fsm& fsm)
    {
        fsm.brightness = event.brightness;
    }
};

// State machine front-end with a reflect member function.
struct DimSwitch_ : front::state_machine_def<DimSwitch_>
{
    using initial_state = Off;

    using transition_table = mp11::mp_list<
        front::Row<Off, TurnOn , On >,
        front::Row<On , TurnOff, Off>
        >;

    using internal_transition_table = mp11::mp_list<
        front::Internal<Dim, SetDimValue>
        >;

    template <typename Visitor>
    void reflect(Visitor&& visitor)
    {
        visitor.visit_member("brightness", this->brightness);
    }

    template <typename Visitor>
    void reflect(Visitor&& visitor) const
    {
        visitor.visit_member("brightness", brightness);
    }

    uint8_t brightness{};
};

using DimSwitch = backmp11::state_machine<DimSwitch_>;

} // namespace

#endif // BOOST_MSM_EXAMPLE_DIM_SWITCH_HPP

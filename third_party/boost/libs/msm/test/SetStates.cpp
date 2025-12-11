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
// set_states API is not supported by backmp11
#define BOOST_MSM_TEST_SKIP_BACKMP11
#include "BackCommon.hpp"
//front-end
#include <boost/msm/front/state_machine_def.hpp>
#include <boost/msm/front/functor_row.hpp>

#ifndef BOOST_MSM_NONSTANDALONE_TEST
#define BOOST_TEST_MODULE set_states_test
#endif
#include <boost/test/unit_test.hpp>

namespace msm = boost::msm;
namespace mpl = boost::mpl;
using namespace msm::front;

namespace
{
    struct InputEvent {};
    struct ExitEvent
    {
        ExitEvent() = default;
        template <typename T>
        ExitEvent(const T&) {};
    };

    struct State : public boost::msm::front::state<>
    {
        template <class Event, class FSM>
        void on_entry(Event const&, FSM&)
        {
            std::cout << "State - on_entry" << "\n";
        };

        template <class Event, class FSM>
        void on_exit(Event const&, FSM&)
        {
            std::cout << "State - on_exit" << "\n";
        }
    };

    struct TerminateState : public boost::msm::front::terminate_state<>
    {
        template <class Event, class FSM>
        void on_entry(Event const&, FSM&)
        {
            std::cout << "TerminateState - on_entry" << "\n";
        };

        template <class Event, class FSM>
        void on_exit(Event const&, FSM&)
        {
            std::cout << "TerminateState - on_exit" << "\n";
        }
    };

    struct PseudoExitState : public boost::msm::front::exit_pseudo_state<ExitEvent>
    {
    };

    template<template <typename...> class Back, typename Policy = void>
    struct hierarchical_state_machine
    {
    struct SubFsm_ : public msm::front::state_machine_def<SubFsm_>
    {
        BOOST_MSM_TEST_DEFINE_DEPENDENT_TEMPLATES(SubFsm_)

        template <class Event, class FSM>
        void on_entry(Event const&, FSM&)
        {
            std::cout << "SubFsm - on_entry" << "\n";
        };

        template <class Event, class FSM>
        void on_exit(Event const&, FSM&)
        {
            std::cout << "SubFsm - on_exit" << "\n";
        }

        typedef State initial_state;

        struct transition_table : mpl::vector<
            Row< State, InputEvent, PseudoExitState, none, none >
        > {};

        template <class FSM, class Event>
        void no_transition(Event const& e, FSM&, int state)
        {
            std::cout << "SubFsm: no transition from state " << state
                << " on event " << typeid(e).name() << std::endl;
        }
    };
    typedef Back<SubFsm_, Policy> SubFsm;

    struct Fsm_ : public msm::front::state_machine_def<Fsm_>
    {
        BOOST_MSM_TEST_DEFINE_DEPENDENT_TEMPLATES(Fsm_)

        template <class Event, class FSM>
        void on_entry(Event const&, FSM&)
        {
            std::cout << "Fsm - on_entry" << "\n";
        };

        template <class Event, class FSM>
        void on_exit(Event const&, FSM&)
        {
            std::cout << "Fsm - on_exit" << "\n";
        }

        typedef SubFsm initial_state;

        struct transition_table : mpl::vector<
            Row< typename SubFsm::template exit_pt<PseudoExitState>, ExitEvent, TerminateState, none, none >
        > {};

        template <class FSM, class Event>
        void no_transition(Event const& e, FSM&, int state)
        {
            std::cout << "Fsm: no transition from state " << state
                << " on event " << typeid(e).name() << std::endl;
        }
    };
    typedef Back<Fsm_, Policy> Fsm;
    };
    // typedef msm::back11::state_machine<Fsm_> Fsm;
    typedef get_hierarchical_test_machines<hierarchical_state_machine> test_machines;


    BOOST_AUTO_TEST_CASE_TEMPLATE(set_states_test, test_machine, test_machines)
    {     
        InputEvent input_event;
        typedef typename test_machine::Fsm Fsm;
        typedef typename test_machine::SubFsm SubFsm;

        std::cout << "\nSubFsms created in constructor\n";
        Fsm fsm_subfsms_created_in_constructor(boost::msm::back::states_
            << SubFsm());
        fsm_subfsms_created_in_constructor.start();
        fsm_subfsms_created_in_constructor.process_event(input_event);

        std::cout << "\nSubFsms created through set_states method\n";
        Fsm fsm_subfsms_created_thorugh_constructor;
        fsm_subfsms_created_thorugh_constructor.set_states(boost::msm::back::states_
            << SubFsm());
        fsm_subfsms_created_thorugh_constructor.start();
        fsm_subfsms_created_thorugh_constructor.process_event(input_event);
    }
}

#if !defined(BOOST_MSM_TEST_ONLY_BACKMP11)
using back0 = hierarchical_state_machine<boost::msm::back::state_machine, boost::msm::back::favor_compile_time>::Fsm;
using back1 = hierarchical_state_machine<boost::msm::back::state_machine, boost::msm::back::favor_compile_time>::SubFsm;
BOOST_MSM_BACK_GENERATE_PROCESS_EVENT(back1);
#endif

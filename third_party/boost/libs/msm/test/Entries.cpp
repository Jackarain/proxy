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
//front-end
#include <boost/msm/front/state_machine_def.hpp>
#ifndef BOOST_MSM_NONSTANDALONE_TEST
#define BOOST_TEST_MODULE entries_test
#endif
#include <boost/test/unit_test.hpp>

namespace msm = boost::msm;
namespace mpl = boost::mpl;

namespace
{
    // events
    struct event1 {};
    struct event2 {};
    struct event3 {};
    struct event4 {};
    struct event5 {};
    struct event6 
    {
        event6(){}
        template <class Event>
        event6(Event const&){}
    };
    template<template <typename...> class Back, typename Policy = void>
    struct hierarchical_state_machine
    {
    // front-end: define the FSM structure 
    struct Fsm_ : public msm::front::state_machine_def<Fsm_>
    {
        BOOST_MSM_TEST_DEFINE_DEPENDENT_TEMPLATES(Fsm_)

        // The list of FSM states
        struct State1 : public msm::front::state<> 
        {
            template <class Event,class FSM>
            void on_entry(Event const&,FSM& ) {++entry_counter;}
            template <class Event,class FSM>
            void on_exit(Event const&,FSM& ) {++exit_counter;}
            int entry_counter;
            int exit_counter;
        };
        struct State2 : public msm::front::state<> 
        {
            template <class Event,class FSM>
            void on_entry(Event const&,FSM& ) {++entry_counter;}
            template <class Event,class FSM>
            void on_exit(Event const&,FSM& ) {++exit_counter;}
            int entry_counter;
            int exit_counter;
        };
        struct SubFsm2_ : public msm::front::state_machine_def<SubFsm2_>
        {
            BOOST_MSM_TEST_DEFINE_DEPENDENT_TEMPLATES(SubFsm2_)

            unsigned int entry_action_counter;

            template <class Event,class FSM>
            void on_entry(Event const&,FSM& ) {++entry_counter;}
            template <class Event,class FSM>
            void on_exit(Event const&,FSM& ) {++exit_counter;}
            int entry_counter;
            int exit_counter;

            struct SubState1 : public msm::front::state<>
            {
                template <class Event,class FSM>
                void on_entry(Event const&,FSM& ) {++entry_counter;}
                template <class Event,class FSM>
                void on_exit(Event const&,FSM& ) {++exit_counter;}
                int entry_counter;
                int exit_counter;
            };
            struct SubState1b : public msm::front::state<>
            {
                template <class Event,class FSM>
                void on_entry(Event const&,FSM& ) {++entry_counter;}
                template <class Event,class FSM>
                void on_exit(Event const&,FSM& ) {++exit_counter;}
                int entry_counter;
                int exit_counter;
            };
            struct SubState2 : public msm::front::state<> , public msm::front::explicit_entry<0>
            {
                template <class Event,class FSM>
                void on_entry(Event const&,FSM& ) {++entry_counter;}
                template <class Event,class FSM>
                void on_exit(Event const&,FSM& ) {++exit_counter;}
                int entry_counter;
                int exit_counter;
            };
            struct SubState2b : public msm::front::state<> , public msm::front::explicit_entry<1>
            {
                template <class Event,class FSM>
                void on_entry(Event const&,FSM& ) {++entry_counter;}
                template <class Event,class FSM>
                void on_exit(Event const&,FSM& ) {++exit_counter;}
                int entry_counter;
                int exit_counter;
            };
            // test with a pseudo entry
            struct PseudoEntry1 : public msm::front::entry_pseudo_state<0>
            {
                template <class Event,class FSM>
                void on_entry(Event const&,FSM& ) {++entry_counter;}
                template <class Event,class FSM>
                void on_exit(Event const&,FSM& ) {++exit_counter;}
                int entry_counter;
                int exit_counter;
            };
            struct SubState3 : public msm::front::state<>
            {
                template <class Event,class FSM>
                void on_entry(Event const&,FSM& ) {++entry_counter;}
                template <class Event,class FSM>
                void on_exit(Event const&,FSM& ) {++exit_counter;}
                int entry_counter;
                int exit_counter;
            };
            struct PseudoExit1 : public msm::front::exit_pseudo_state<event6> 
            {
                template <class Event,class FSM>
                void on_entry(Event const&,FSM& ) {++entry_counter;}
                template <class Event,class FSM>
                void on_exit(Event const&,FSM& ) {++exit_counter;}
                int entry_counter;
                int exit_counter;
            };
            // action methods
            void entry_action(event4 const&)
            {
                ++entry_action_counter;
            }
            // the initial state. Must be defined
            typedef mpl::vector<SubState1,SubState1b> initial_state;

            typedef mpl::vector<SubState2b> explicit_creation;

            // Transition table for SubFsm2
            struct transition_table : mpl::vector<
                //      Start          Event         Next         Action                  Guard
                //    +--------------+-------------+------------+------------------------+----------------------+
                a_row < PseudoEntry1 , event4      , SubState3  ,&SubFsm2_::entry_action                        >,
                _row  < SubState2    , event6      , SubState1                                                  >,
                _row  < SubState3    , event5      , PseudoExit1                                                >
                //    +--------------+-------------+------------+------------------------+----------------------+
            > {};
            // Replaces the default no-transition response.
            template <class FSM,class Event>
            void no_transition(Event const& , FSM&,int)
            {
                BOOST_FAIL("no_transition called!");
            }
        };
        typedef Back<SubFsm2_, Policy> SubFsm2;

        // the initial state of the player SM. Must be defined
        typedef State1 initial_state;

        // transition actions
        // guard conditions

        // Transition table for Fsm
        struct transition_table : mpl::vector<
            //    Start                 Event    Next                                                  Action  Guard
            //   +---------------------+--------+-----------------------------------------------------+-------+--------+
            _row < State1              , event1 , SubFsm2                                                               >,
            _row < State1              , event2 , typename SubFsm2::template direct<typename SubFsm2_::SubState2>                >,
            _row < State1              , event3 , mpl::vector<typename SubFsm2::template direct<typename SubFsm2_::SubState2>,
                                                              typename SubFsm2::template direct<typename SubFsm2_::SubState2b> > >,
            _row < State1              , event4 , typename SubFsm2::template entry_pt
                                                        <typename SubFsm2_::PseudoEntry1>                                        >,
            //   +---------------------+--------+-----------------------------------------------------+-------+--------+
            _row < SubFsm2             , event1 , State1                                                                >,
            _row < typename SubFsm2::template exit_pt
                <typename SubFsm2_::PseudoExit1>, event6 , State2                                                                >
            //   +---------------------+--------+-----------------------------------------------------+-------+--------+
        > {};

        // Replaces the default no-transition response.
        template <class FSM,class Event>
        void no_transition(Event const& , FSM&,int )
        {
            BOOST_FAIL("no_transition called!");
        }
        // init counters
        template <class Event,class FSM>
        void on_entry(Event const&,FSM& fsm) 
        {
            fsm.template get_state<Fsm_::State1&>().entry_counter=0;
            fsm.template get_state<Fsm_::State1&>().exit_counter=0;
            fsm.template get_state<Fsm_::State2&>().entry_counter=0;
            fsm.template get_state<Fsm_::State2&>().exit_counter=0;
            fsm.template get_state<Fsm_::SubFsm2&>().entry_counter=0;
            fsm.template get_state<Fsm_::SubFsm2&>().exit_counter=0;
            fsm.template get_state<Fsm_::SubFsm2&>().entry_action_counter=0;
            fsm.template get_state<Fsm_::SubFsm2&>().template get_state<typename Fsm_::SubFsm2::SubState1&>().entry_counter=0;
            fsm.template get_state<Fsm_::SubFsm2&>().template get_state<typename Fsm_::SubFsm2::SubState1&>().exit_counter=0;
            fsm.template get_state<Fsm_::SubFsm2&>().template get_state<typename Fsm_::SubFsm2::SubState1b&>().entry_counter=0;
            fsm.template get_state<Fsm_::SubFsm2&>().template get_state<typename Fsm_::SubFsm2::SubState1b&>().exit_counter=0;
            fsm.template get_state<Fsm_::SubFsm2&>().template get_state<typename Fsm_::SubFsm2::SubState2&>().entry_counter=0;
            fsm.template get_state<Fsm_::SubFsm2&>().template get_state<typename Fsm_::SubFsm2::SubState2&>().exit_counter=0;
            fsm.template get_state<Fsm_::SubFsm2&>().template get_state<typename Fsm_::SubFsm2::SubState2b&>().entry_counter=0;
            fsm.template get_state<Fsm_::SubFsm2&>().template get_state<typename Fsm_::SubFsm2::SubState2b&>().exit_counter=0;
            fsm.template get_state<Fsm_::SubFsm2&>().template get_state<typename Fsm_::SubFsm2::SubState3&>().entry_counter=0;
            fsm.template get_state<Fsm_::SubFsm2&>().template get_state<typename Fsm_::SubFsm2::SubState3&>().exit_counter=0;
            fsm.template get_state<Fsm_::SubFsm2&>().template get_state<typename Fsm_::SubFsm2::PseudoEntry1&>().entry_counter=0;
            fsm.template get_state<Fsm_::SubFsm2&>().template get_state<typename Fsm_::SubFsm2::PseudoEntry1&>().exit_counter=0;
            fsm.template get_state<Fsm_::SubFsm2&>().template get_state<typename Fsm_::SubFsm2::template exit_pt<typename SubFsm2_::PseudoExit1>&>().entry_counter=0;
            fsm.template get_state<Fsm_::SubFsm2&>().template get_state<typename Fsm_::SubFsm2::template exit_pt<typename SubFsm2_::PseudoExit1>&>().exit_counter=0;

        }
    };
    typedef Back<Fsm_, Policy> Fsm;
    };

    typedef get_hierarchical_test_machines<hierarchical_state_machine> test_machines;
//    static char const* const state_names[] = { "State1", "SubFsm2","State2"  };


    BOOST_AUTO_TEST_CASE_TEMPLATE( entries_test, test_machine, test_machines )
    {
        typename test_machine::Fsm p;
        typedef typename test_machine::Fsm_ Fsm_;

        p.start(); 
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::State1&>().entry_counter == 1,"State1 entry not called correctly");

        p.process_event(event1()); 
        p.process_event(event1()); 
        BOOST_CHECK_MESSAGE(p.current_state()[0] == 0,"State1 should be active");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::State1&>().exit_counter == 1,"State1 exit not called correctly");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::State1&>().entry_counter == 2,"State1 entry not called correctly");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::SubFsm2&>().exit_counter == 1,"SubFsm2 exit not called correctly");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::SubFsm2&>().entry_counter == 1,"SubFsm2 entry not called correctly");

        p.process_event(event2()); 
        p.process_event(event6()); 
        p.process_event(event1()); 
        BOOST_CHECK_MESSAGE(p.current_state()[0] == 0,"State1 should be active");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::State1&>().exit_counter == 2,"State1 exit not called correctly");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::State1&>().entry_counter == 3,"State1 entry not called correctly");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::SubFsm2&>().exit_counter == 2,"SubFsm2 exit not called correctly");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::SubFsm2&>().entry_counter == 2,"SubFsm2 entry not called correctly");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::SubFsm2&>().template get_state<typename Fsm_::SubFsm2_::SubState2&>().entry_counter == 1,
                            "SubFsm2::SubState2 entry not called correctly");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::SubFsm2&>().template get_state<typename Fsm_::SubFsm2_::SubState2&>().exit_counter == 1,
                            "SubFsm2::SubState2 exit not called correctly");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::SubFsm2&>().template get_state<typename Fsm_::SubFsm2_::SubState1&>().entry_counter == 2,
                            "SubFsm2::SubState1 entry not called correctly");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::SubFsm2&>().template get_state<typename Fsm_::SubFsm2_::SubState1&>().exit_counter == 2,
                            "SubFsm2::SubState1 exit not called correctly");

        p.process_event(event3()); 
        p.process_event(event1()); 
        BOOST_CHECK_MESSAGE(p.current_state()[0] == 0,"State1 should be active");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::State1&>().exit_counter == 3,"State1 exit not called correctly");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::State1&>().entry_counter == 4,"State1 entry not called correctly");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::SubFsm2&>().exit_counter == 3,"SubFsm2 exit not called correctly");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::SubFsm2&>().entry_counter == 3,"SubFsm2 entry not called correctly");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::SubFsm2&>().template get_state<typename Fsm_::SubFsm2_::SubState2&>().entry_counter == 2,
                            "SubFsm2::SubState2 entry not called correctly");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::SubFsm2&>().template get_state<typename Fsm_::SubFsm2_::SubState2&>().exit_counter == 2,
                            "SubFsm2::SubState2 exit not called correctly");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::SubFsm2&>().template get_state<typename Fsm_::SubFsm2_::SubState2b&>().entry_counter == 1,
                            "SubFsm2::SubState2b entry not called correctly");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::SubFsm2&>().template get_state<typename Fsm_::SubFsm2_::SubState2b&>().exit_counter == 1,
                            "SubFsm2::SubState2b exit not called correctly");

        p.process_event(event4()); 
        p.process_event(event5()); 
        BOOST_CHECK_MESSAGE(p.current_state()[0] == 2,"State2 should be active");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::State1&>().exit_counter == 4,"State1 exit not called correctly");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::State2&>().entry_counter == 1,"State2 entry not called correctly");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::SubFsm2&>().exit_counter == 4,"SubFsm2 exit not called correctly");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::SubFsm2&>().entry_counter == 4,"SubFsm2 entry not called correctly");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::SubFsm2&>().template get_state<typename Fsm_::SubFsm2_::PseudoEntry1&>().entry_counter == 1,
                            "SubFsm2::PseudoEntry1 entry not called correctly");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::SubFsm2&>().template get_state<typename Fsm_::SubFsm2_::PseudoEntry1&>().exit_counter == 1,
                            "SubFsm2::PseudoEntry1 exit not called correctly");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::SubFsm2&>().template get_state<typename Fsm_::SubFsm2_::SubState3&>().entry_counter == 1,
                            "SubFsm2::SubState3 entry not called correctly");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::SubFsm2&>().template get_state<typename Fsm_::SubFsm2_::SubState3&>().exit_counter == 1,
                            "SubFsm2::SubState3 exit not called correctly");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::SubFsm2&>().template get_state<typename Fsm_::SubFsm2::template exit_pt<typename Fsm_::SubFsm2_::PseudoExit1>&>().entry_counter == 1,
                            "SubFsm2::PseudoExit1 entry not called correctly");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::SubFsm2&>().template get_state<typename Fsm_::SubFsm2::template exit_pt<typename Fsm_::SubFsm2_::PseudoExit1>&>().exit_counter == 1,
                            "SubFsm2::PseudoExit1 exit not called correctly");
        BOOST_CHECK_MESSAGE(p.template get_state<typename Fsm_::SubFsm2&>().entry_action_counter == 1,"Action not called correctly");

    }
}

#if !defined(BOOST_MSM_TEST_ONLY_BACKMP11)
using back0 = hierarchical_state_machine<boost::msm::back::state_machine, boost::msm::back::favor_compile_time>::Fsm;
using back1 = hierarchical_state_machine<boost::msm::back::state_machine, boost::msm::back::favor_compile_time>::Fsm_::SubFsm2;
BOOST_MSM_BACK_GENERATE_PROCESS_EVENT(back1);
#endif

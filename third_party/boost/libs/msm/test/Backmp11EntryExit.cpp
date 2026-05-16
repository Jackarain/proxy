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
#define BOOST_TEST_MODULE backmp11_entry_exit_test
#endif
#include <boost/test/unit_test.hpp>

// back-end
#include "BackCommon.hpp"
//front-end
#include "FrontCommon.hpp"

namespace mp11 = boost::mp11;
using namespace boost::msm::front;
using namespace boost::msm::backmp11;

namespace
{

// events
struct EnterSubmachine {};
struct ExitSubmachine {};
struct EnterSubmachineExplicitEntry {};
struct EnterSubmachineForkEntry {};
struct EnterSubmachinePseudoEntry {};
struct ExitSubmachinePseudoExit {};
struct SomeEvent 
{
    SomeEvent(){}
    template <class Event>
    SomeEvent(Event const&){}
};

// states
template <typename ExpectedFsm>
struct StateBase : test::StateBase
{
    template <class Event, class Fsm>
    void on_entry(const Event& event, Fsm& fsm)
    {
        static_assert(std::is_same_v<Fsm, ExpectedFsm>);
        test::StateBase::on_entry(event, fsm);
    }

    template <class Event, class Fsm>
    void on_exit(const Event& event, Fsm& fsm)
    {
        static_assert(std::is_same_v<Fsm, ExpectedFsm>);
        test::StateBase::on_exit(event, fsm);
    }
};

template<typename Config = default_state_machine_config>
struct hierarchical_state_machine
{
    // Forward declarations required for static assertions on FSM parameters.
    struct Machine_;
    using Machine = state_machine<Machine_, Config>;
    struct Submachine_;
    using Submachine = state_machine<Submachine_, Config>;

    template <typename FrontEnd>
    struct StateMachineBase_ : test::StateMachineBase_<FrontEnd>
    {
        template <class Event, class Fsm>
        void on_entry(const Event& event, Fsm& fsm)
        {
            // Both machines should receive the parent SM as Fsm argument.
            static_assert(std::is_same_v<Fsm, Machine>);
            test::StateMachineBase_<FrontEnd>::on_entry(event, fsm);
        }

        template <class Event, class Fsm>
        void on_exit(const Event& event, Fsm& fsm)
        {
            // Both machines should receive the parent SM as Fsm argument.
            static_assert(std::is_same_v<Fsm, Machine>);
            test::StateMachineBase_<FrontEnd>::on_exit(event, fsm);
        }
    };

    struct Submachine_ : StateMachineBase_<Submachine_>
    {
        BOOST_MSM_TEST_DEFINE_DEPENDENT_TEMPLATES(Submachine_)

        using StateBase = ::StateBase<Submachine>;

        struct Substate0 : StateBase {};
        struct Substate1 : StateBase {};
        struct ExplicitEntry0 : StateBase, explicit_entry<0> {};
        struct ExplicitEntry1 : StateBase, explicit_entry<1> {};
        struct Substate2 : StateBase {};

        // TODO:
        // According to UML a pseudostate shouldn't have entry/exit behavior.
        struct PseudoEntry0 : entry_pseudo_state<0>
        {
            template <class Event, class Fsm>
            void on_entry(const Event&, Fsm&)
            {
                static_assert(std::is_same_v<Fsm, Submachine>);
                entry_counter++;
            }
            
            template <class Event, class Fsm>
            void on_exit(const Event&, Fsm&)
            {
                static_assert(std::is_same_v<Fsm, Submachine>);
                exit_counter++;
            }

            size_t entry_counter{};
            size_t exit_counter{};
        };
        
        // TODO:
        // According to UML a pseudostate shouldn't have entry/exit behavior.
        struct PseudoExit0 : exit_pseudo_state<SomeEvent> 
        {
            template <class Event, class Fsm>
            void on_entry(const Event&, Fsm&)
            {
                static_assert(std::is_same_v<Fsm, Submachine>);
                entry_counter++;
            }
            
            template <class Event, class Fsm>
            void on_exit(const Event&, Fsm&)
            {
                static_assert(std::is_same_v<Fsm, Submachine>);
                exit_counter++;
            }

            size_t entry_counter{};
            size_t exit_counter{};
        };

        struct AssertEnterSubmachinePseudoEntry
        {
            template <typename Event, typename Fsm, typename Source, typename Target>
            void operator()(const Event&, Fsm&, Source&, Target&)
            {
                static_assert(std::is_same_v<Source, PseudoEntry0>);
                static_assert(std::is_same_v<Target, Substate2>);
            }
        };

        struct AssertExitSubmachinePseudoExit1
        {
            template <typename Event, typename Fsm, typename Source, typename Target>
            void operator()(const Event&, Fsm&, Source&, Target&)
            {
                static_assert(std::is_same_v<Source, Substate2>);
                // TODO:
                // Should the action receive PseudoExit0 instead of Machine::exit_pt<PseudoExit0>?
                // static_assert(std::is_same_v<Target, PseudoExit0>);
            }
        };
        
        using initial_state = mp11::mp_list<Substate0, Substate1>;
        using explicit_creation = mp11::mp_list<ExplicitEntry1>;

        using transition_table = mp11::mp_list<
            Row < PseudoEntry0   , EnterSubmachinePseudoEntry , Substate2   , AssertEnterSubmachinePseudoEntry >,
            Row < ExplicitEntry0 , SomeEvent                  , Substate0                                      >,
            Row < Substate2      , ExitSubmachinePseudoExit   , PseudoExit0 , AssertExitSubmachinePseudoExit1  >
        >;
    };

    struct Machine_ : StateMachineBase_<Machine_>
    {
        BOOST_MSM_TEST_DEFINE_DEPENDENT_TEMPLATES(Machine_)

        using StateBase = ::StateBase<Machine>;

        struct State0 : StateBase {};

        using SubmachineEntryPt =
            typename Submachine::template entry_pt<typename Submachine_::PseudoEntry0>;
        using SubmachineExplicitEntryPt =
            typename Submachine::template direct<typename Submachine_::ExplicitEntry0>;
        using SubmachineForkEntryPt = 
            mp11::mp_list<typename Submachine::template direct<typename Submachine_::ExplicitEntry0>,
                          typename Submachine::template direct<typename Submachine_::ExplicitEntry1>>;
        using SubmachineExitPt = 
            typename Submachine::template exit_pt<typename Submachine_::PseudoExit0>;

        struct AssertEnterSubmachine
        {
            template <typename Event, typename Fsm, typename Source, typename Target>
            void operator()(const Event&, Fsm&, Source&, Target&)
            {
                static_assert(std::is_same_v<Source, State0>);
                static_assert(std::is_same_v<Target, Submachine>);
            }
        };

        struct AssertEnterSubmachinePseudoEntry
        {
            template <typename Event, typename Fsm, typename Source, typename Target>
            void operator()(const Event&, Fsm&, Source&, Target&)
            {
                static_assert(std::is_same_v<Source, State0>);
                // TODO:
                // Should be PseudoEntry0 instead of Submachine.
                static_assert(std::is_same_v<Target, Submachine>);
            }
        };

        struct AssertEnterSubmachineExplicitEntry
        {
            template <typename Event, typename Fsm, typename Source, typename Target>
            void operator()(const Event&, Fsm&, Source&, Target&)
            {
                static_assert(std::is_same_v<Source, State0>);
                // TODO:
                // Should be ExplicitEntry0 instead of Submachine.
                // static_assert(std::is_same_v<Target, typename Submachine_::ExplicitEntry0>);
                static_assert(std::is_same_v<Target, Submachine>);
            }
        };

        struct AssertEnterSubmachineForkEntry
        {
            template <typename Event, typename Fsm, typename Source, typename Target>
            void operator()(const Event&, Fsm&, Source&, Target&)
            {
                static_assert(std::is_same_v<Source, State0>);
                // TODO:
                // Should be mp11::mp_list<ExplicitEntry0, ExplicitEntry1> instead of Submachine (??).
                static_assert(std::is_same_v<Target, Submachine>);
            }
        };

        using initial_state = State0;

        using transition_table = mp11::mp_list<
            Row < State0           , EnterSubmachine              , Submachine                , AssertEnterSubmachine              >,
            Row < State0           , EnterSubmachinePseudoEntry   , SubmachineEntryPt         , AssertEnterSubmachinePseudoEntry   >,
            Row < State0           , EnterSubmachineExplicitEntry , SubmachineExplicitEntryPt , AssertEnterSubmachineExplicitEntry >,
            Row < State0           , EnterSubmachineForkEntry     , SubmachineForkEntryPt     , AssertEnterSubmachineForkEntry>,
            Row < Submachine       , ExitSubmachine               , State0                    >,
            Row < SubmachineExitPt , SomeEvent                    , State0                    >
        >;
    };
};

using TestMachines = mp11::mp_list<
    hierarchical_state_machine<>,
    hierarchical_state_machine<favor_compile_time_config>
    >;

#define CHECK_AND_RESET_COUNTER(counter, expected)                             \
    {                                                                          \
        BOOST_REQUIRE(counter == expected);                                    \
        counter = 0;                                                           \
    }

BOOST_AUTO_TEST_CASE_TEMPLATE(backmp11_entry_exit_test, test_machine, TestMachines)
{
    using Machine = typename test_machine::Machine;
    using Machine_ = typename test_machine::Machine_;
    Machine p;

    using State0 = typename Machine_::State0;

    using Submachine = typename test_machine::Submachine;
    using Substate0 = typename Submachine::Substate0;
    using Substate1 = typename Submachine::Substate1;
    using Substate2 = typename Submachine::Substate2;
    using PseudoEntry0 = typename Submachine::PseudoEntry0;
    using ExplicitEntry0 = typename Submachine::ExplicitEntry0;
    using ExplicitEntry1 = typename Submachine::ExplicitEntry1;
    // TODO:
    // Can we define PseudoExit0 as seen from the submachine?
    using PseudoExit0 = typename Machine_::SubmachineExitPt;

    
    auto& state_0 = p.template get_state<State0>();

    auto& submachine = p.template get_state<Submachine&>();
    auto& substate_0 = submachine.template get_state<Substate0>();
    auto& substate_1 = submachine.template get_state<Substate1>();
    auto& substate_2 = submachine.template get_state<Substate2>();
    auto& pseudo_entry_0 = submachine.template get_state<PseudoEntry0>();
    auto& explicit_entry_0 = submachine.template get_state<ExplicitEntry0>();
    auto& explicit_entry_1 = submachine.template get_state<ExplicitEntry1>();
    auto& pseudo_exit_0 = submachine.template get_state<PseudoExit0>();

    p.start(); 
    BOOST_REQUIRE(p.template is_state_active<State0>());
    CHECK_AND_RESET_COUNTER(state_0.entry_counter, 1);

    // Normal entry/exit: [Substate0, Substate1].
    p.process_event(EnterSubmachine{});
    BOOST_REQUIRE(p.template is_state_active<Submachine>());
    BOOST_REQUIRE(p.template is_state_active<Substate0>());
    BOOST_REQUIRE(p.template is_state_active<Substate1>());
    CHECK_AND_RESET_COUNTER(state_0.exit_counter, 1);
    CHECK_AND_RESET_COUNTER(submachine.entry_counter, 1);
    CHECK_AND_RESET_COUNTER(substate_0.entry_counter, 1);
    CHECK_AND_RESET_COUNTER(substate_1.entry_counter, 1);
    p.process_event(ExitSubmachine{});
    BOOST_REQUIRE(p.template is_state_active<State0>());
    CHECK_AND_RESET_COUNTER(state_0.entry_counter, 1);
    CHECK_AND_RESET_COUNTER(submachine.exit_counter, 1);
    CHECK_AND_RESET_COUNTER(substate_0.exit_counter, 1);
    CHECK_AND_RESET_COUNTER(substate_1.exit_counter, 1);

    // Pseudo entry: [Substate1, Substate2] via PseudoEntry0.
    p.process_event(EnterSubmachinePseudoEntry{});
    BOOST_REQUIRE(p.template is_state_active<Substate1>());
    BOOST_REQUIRE(p.template is_state_active<Substate2>());
    CHECK_AND_RESET_COUNTER(state_0.exit_counter, 1);
    CHECK_AND_RESET_COUNTER(submachine.entry_counter, 1);
    CHECK_AND_RESET_COUNTER(pseudo_entry_0.entry_counter, 1);
    CHECK_AND_RESET_COUNTER(pseudo_entry_0.exit_counter, 1);
    CHECK_AND_RESET_COUNTER(substate_2.entry_counter, 1);
    CHECK_AND_RESET_COUNTER(substate_1.entry_counter, 1);

    // Pseudo exit: Transitions via PseudoExit0.
    p.process_event(ExitSubmachinePseudoExit{});
    BOOST_REQUIRE(p.template is_state_active<State0>());
    CHECK_AND_RESET_COUNTER(pseudo_exit_0.entry_counter, 1);
    CHECK_AND_RESET_COUNTER(pseudo_exit_0.exit_counter, 1);
    CHECK_AND_RESET_COUNTER(submachine.exit_counter, 1);
    CHECK_AND_RESET_COUNTER(substate_1.exit_counter, 1);
    CHECK_AND_RESET_COUNTER(substate_2.exit_counter, 1);
    CHECK_AND_RESET_COUNTER(state_0.entry_counter, 1);

    // Explicit entry: [ExplicitEntry0, SubState1].
    p.process_event(EnterSubmachineExplicitEntry{});
    BOOST_REQUIRE(p.template is_state_active<ExplicitEntry0>());
    BOOST_REQUIRE(p.template is_state_active<Substate1>());
    CHECK_AND_RESET_COUNTER(state_0.exit_counter, 1);
    CHECK_AND_RESET_COUNTER(submachine.entry_counter, 1);
    CHECK_AND_RESET_COUNTER(explicit_entry_0.entry_counter, 1);
    CHECK_AND_RESET_COUNTER(substate_1.entry_counter, 1);
    p.process_event(ExitSubmachine{});
    CHECK_AND_RESET_COUNTER(state_0.entry_counter, 1);
    CHECK_AND_RESET_COUNTER(submachine.exit_counter, 1);
    CHECK_AND_RESET_COUNTER(explicit_entry_0.exit_counter, 1);
    CHECK_AND_RESET_COUNTER(substate_1.exit_counter, 1);

    // Explicit entry with fork: [ExplicitEntry0, ExplicitEntry1].
    p.process_event(EnterSubmachineForkEntry{});
    BOOST_REQUIRE(p.template is_state_active<ExplicitEntry0>());
    BOOST_REQUIRE(p.template is_state_active<ExplicitEntry1>());
    CHECK_AND_RESET_COUNTER(state_0.exit_counter, 1);
    CHECK_AND_RESET_COUNTER(submachine.entry_counter, 1);
    CHECK_AND_RESET_COUNTER(explicit_entry_0.entry_counter, 1);
    CHECK_AND_RESET_COUNTER(explicit_entry_1.entry_counter, 1);
    p.process_event(ExitSubmachine{});
    CHECK_AND_RESET_COUNTER(state_0.entry_counter, 1);
    CHECK_AND_RESET_COUNTER(submachine.exit_counter, 1);
    CHECK_AND_RESET_COUNTER(explicit_entry_0.exit_counter, 1);
    CHECK_AND_RESET_COUNTER(explicit_entry_1.exit_counter, 1);
}

} // namespace

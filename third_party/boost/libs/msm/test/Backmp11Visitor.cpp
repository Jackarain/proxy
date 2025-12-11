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
// Generate the favor_compile_time SM manually.
#define BOOST_MSM_BACKMP11_MANUAL_GENERATION
#include <boost/msm/backmp11/favor_compile_time.hpp>
#include <boost/msm/backmp11/state_machine.hpp>
//front-end
#include <boost/msm/front/state_machine_def.hpp>
#include <boost/msm/front/functor_row.hpp>

#ifndef BOOST_MSM_NONSTANDALONE_TEST
#define BOOST_TEST_MODULE backmp11_visitor_test
#endif
#include <boost/test/unit_test.hpp>

#include "Backmp11.hpp"

namespace msm = boost::msm;
namespace mp11 = boost::mp11;

using namespace msm::front;
using namespace msm::backmp11;

namespace
{
    class CountVisitor
    {
      public:
        CountVisitor(int& visits) : m_visits(visits) {}

        template<typename State>
        void operator()(State& state)
        {
            m_visits += 1;
            state.visits += 1;
        }

      private:
        int& m_visits;
    };

    class ResetCountVisitor
    {
      public:
        ResetCountVisitor(int& visits) : m_visits(visits) {}

        template<typename State>
        void operator()(State& state)
        {
            m_visits = 0;
            state.visits = 0;
        }

      private:
        int& m_visits;
    };

    class NoOpVisitor
    {
      public:
        template<typename State>
        void operator()(State& /*state*/) {}
    };

    class NoOpConstVisitor
    {
      public:
        template<typename State>
        void operator()(State& /*state*/) const {}
    };

    // Events.
    struct EnterSubFsm{};
    struct ExitSubFsm{};

    // States.
    struct StateBase
    {
        size_t visits;
    };

    struct DefaultState : public state<StateBase> {};

    template<typename T>
    struct MachineBase_ : public state_machine_def<MachineBase_<T>>
    {
        using initial_state = DefaultState;

        size_t visits;
    };

    template<typename Config = default_state_machine_config>
    struct hierarchical_state_machine
    {
        struct LowerMachine_ : public MachineBase_<LowerMachine_> { };
        using LowerMachine = state_machine<LowerMachine_, Config>;

        struct MiddleMachine_ : public MachineBase_<MiddleMachine_>
        {
            using transition_table = mp11::mp_list<
                Row< DefaultState , EnterSubFsm , LowerMachine >,
                Row< LowerMachine , ExitSubFsm  , DefaultState >
            >;
        };
        using MiddleMachine = state_machine<MiddleMachine_, Config>;

        struct UpperMachine_ : public MachineBase_<UpperMachine_>
        {
            using transition_table = mp11::mp_list<
                Row< DefaultState  , EnterSubFsm , MiddleMachine>,
                Row< MiddleMachine , ExitSubFsm  , DefaultState>
            >;
        };
        using UpperMachine = state_machine<UpperMachine_, Config>;
    };

    using TestMachines = boost::mpl::vector<
        hierarchical_state_machine<>,
        hierarchical_state_machine<favor_compile_time_config>
        >;
    
    BOOST_AUTO_TEST_CASE_TEMPLATE( backmp11_visitor_test, TestMachine, TestMachines )
    {
        using UpperMachine = typename TestMachine::UpperMachine;
        using MiddleMachine = typename TestMachine::MiddleMachine;
        using LowerMachine = typename TestMachine::LowerMachine;

        int visits = 0;
        ResetCountVisitor reset_count_visitor{visits};
        CountVisitor count_visitor{visits};

        UpperMachine upper_machine{};
        auto& upper_machine_state = upper_machine.template get_state<DefaultState&>();
        auto& middle_machine = upper_machine.template get_state<MiddleMachine&>();
        auto& middle_machine_state = middle_machine.template get_state<DefaultState&>();
        auto& lower_machine = middle_machine.template get_state<LowerMachine&>();
        auto& lower_machine_state = lower_machine.template get_state<DefaultState&>();

        // Test visitor signature & compilation (non-const sm).
        {
            upper_machine.visit(NoOpConstVisitor{});
            upper_machine.visit(NoOpVisitor{});
            
            NoOpConstVisitor v0{};
            NoOpVisitor v1{};
            upper_machine.visit(v0);
            upper_machine.visit(v1);

            const NoOpConstVisitor v0_const{};
            upper_machine.visit(v0_const);

            upper_machine.visit([](auto&) mutable {});
        }

        // Test visitor signature & compilation (const sm).
        // Requires more refactoring, visitor_dispatch_table
        // with a Sm template param to preserve constness.
        // {
        //     const UpperMachine& upper_machine_c{upper_machine};
        //     upper_machine_c.visit(NoOpConstVisitor{});
        //     upper_machine_c.visit(NoOpVisitor{});
            
        //     NoOpConstVisitor v0{};
        //     NoOpVisitor v1{};
        //     upper_machine_c.visit(v0);
        //     upper_machine_c.visit(v1);

        //     const NoOpConstVisitor v0_c{};
        //     upper_machine_c.visit(v0_c);

        //     upper_machine_c.visit([](auto&) mutable {});
        // }

        // Test active states
        {
            upper_machine.visit(count_visitor);
            BOOST_REQUIRE(visits == 0);
            
            upper_machine.start();
            BOOST_REQUIRE(upper_machine.template is_state_active<DefaultState>() == true);
            BOOST_REQUIRE(upper_machine.template is_state_active<MiddleMachine>() == false);
            upper_machine.visit(count_visitor);
            BOOST_REQUIRE(upper_machine_state.visits == 1);
            BOOST_REQUIRE(visits == 1);
            upper_machine.visit(reset_count_visitor);

            upper_machine.process_event(EnterSubFsm());
            BOOST_REQUIRE(upper_machine.template is_state_active<DefaultState>() == true);
            BOOST_REQUIRE(upper_machine.template is_state_active<MiddleMachine>() == true);
            BOOST_REQUIRE(middle_machine.template is_state_active<DefaultState>() == true);
            BOOST_REQUIRE(middle_machine.template is_state_active<LowerMachine>() == false);
            upper_machine.visit(count_visitor);
            BOOST_REQUIRE(middle_machine.visits == 1);
            BOOST_REQUIRE(middle_machine_state.visits == 1);
            BOOST_REQUIRE(visits == 2);
            upper_machine.visit(reset_count_visitor);

            upper_machine.process_event(EnterSubFsm());
            BOOST_REQUIRE(upper_machine.template is_state_active<MiddleMachine>() == true);
            BOOST_REQUIRE(upper_machine.template is_state_active<LowerMachine>() == true);
            BOOST_REQUIRE(middle_machine.template is_state_active<LowerMachine>() == true);
            BOOST_REQUIRE(lower_machine.template is_state_active<DefaultState>() == true);
            upper_machine.visit(count_visitor);
            BOOST_REQUIRE(middle_machine.visits == 1);
            BOOST_REQUIRE(lower_machine.visits == 1);
            BOOST_REQUIRE(lower_machine_state.visits == 1);
            BOOST_REQUIRE(visits == 3);
            upper_machine.visit(reset_count_visitor);

            upper_machine.stop();
            upper_machine.visit(count_visitor);
            BOOST_REQUIRE(visits == 0);
        }
        using vm = msm::backmp11::visit_mode;

        // Test all states
        {
            upper_machine.template visit<vm::all_states>(count_visitor);
            BOOST_CHECK(upper_machine_state.visits == 1);
            BOOST_CHECK(middle_machine.visits == 1);
            BOOST_CHECK(visits == 2);
            upper_machine.template visit<vm::all_states>(reset_count_visitor);

            upper_machine.template visit<vm::all_recursive>(count_visitor);
            BOOST_CHECK(upper_machine_state.visits == 1);
            BOOST_CHECK(middle_machine.visits == 1);
            BOOST_CHECK(middle_machine_state.visits == 1);
            BOOST_CHECK(lower_machine.visits == 1);
            BOOST_CHECK(lower_machine_state.visits == 1);
            BOOST_CHECK(visits == 5);
        }
    }
}

using TestMachine = hierarchical_state_machine<favor_compile_time_config>;
BOOST_MSM_BACKMP11_GENERATE_STATE_MACHINE(TestMachine::UpperMachine);
BOOST_MSM_BACKMP11_GENERATE_STATE_MACHINE(TestMachine::MiddleMachine);
BOOST_MSM_BACKMP11_GENERATE_STATE_MACHINE(TestMachine::LowerMachine);

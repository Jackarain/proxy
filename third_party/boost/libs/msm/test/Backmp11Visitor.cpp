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
#define BOOST_TEST_MODULE backmp11_visitor_test
#endif
#include <boost/test/unit_test.hpp>

// back-end
// Generate the favor_compile_time SM manually.
#define BOOST_MSM_BACKMP11_MANUAL_GENERATION
#include "Backmp11.hpp"
// front-end
#include "FrontCommon.hpp"

namespace msm = boost::msm;
namespace mp11 = boost::mp11;

using namespace msm::front;
using namespace msm::backmp11;

using vm = msm::backmp11::visit_mode;

template <vm Mode>
using vm_constant = std::integral_constant<vm, Mode>;

namespace
{

// Events.
struct EnterSubFsm {};
struct ExitSubFsm {};

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
    struct LowerMachine_ : public MachineBase_<LowerMachine_> {};
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

BOOST_AUTO_TEST_CASE_TEMPLATE(visitor, TestMachine, TestMachines)
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

    // Test all states
    {
        upper_machine.template visit<vm::all_non_recursive>(count_visitor);
        BOOST_CHECK(upper_machine_state.visits == 1);
        BOOST_CHECK(middle_machine.visits == 1);
        BOOST_CHECK(visits == 2);
        upper_machine.template visit<vm::all_non_recursive>(reset_count_visitor);

        upper_machine.template visit<vm::all_recursive>(count_visitor);
        BOOST_CHECK(upper_machine_state.visits == 1);
        BOOST_CHECK(middle_machine.visits == 1);
        BOOST_CHECK(middle_machine_state.visits == 1);
        BOOST_CHECK(lower_machine.visits == 1);
        BOOST_CHECK(lower_machine_state.visits == 1);
        BOOST_CHECK(visits == 5);
    }
}

class NoOpVisitor
{
   public:
    NoOpVisitor() = default;

    NoOpVisitor(const NoOpVisitor&)
    {
        copies_or_moves += 1;
    }

    NoOpVisitor& operator=(const NoOpVisitor&)
    {
        copies_or_moves += 1;
        return *this;
    }

    NoOpVisitor(NoOpVisitor&&)
    {
        copies_or_moves += 1;
    }

    NoOpVisitor& operator=(NoOpVisitor&&)
    {
        copies_or_moves += 1;
        return *this;
    }

    template<typename State>
    void operator()(State&) {}

   inline static size_t copies_or_moves{};
};

class NoOpConstVisitor
{
  public:
    NoOpConstVisitor() = default;

    NoOpConstVisitor(const NoOpConstVisitor&)
    {
        copies_or_moves += 1;
    }

    NoOpConstVisitor& operator=(const NoOpConstVisitor&)
    {
        copies_or_moves += 1;
        return *this;
    }

    NoOpConstVisitor(NoOpConstVisitor&&)
    {
        copies_or_moves += 1;
    }

    NoOpConstVisitor& operator=(NoOpConstVisitor&&)
    {
        copies_or_moves += 1;
        return *this;
    }

    template<typename State>
    void operator()(const State&) const {}

    inline static size_t copies_or_moves{};
};

BOOST_AUTO_TEST_CASE_TEMPLATE(no_copy_no_move, TestMachine, TestMachines)
{
    using UpperMachine = typename TestMachine::UpperMachine;

    UpperMachine upper_machine{};

    using visit_modes = mp11::mp_list<
        vm_constant<vm::active_non_recursive>,
        vm_constant<vm::active_recursive>,
        vm_constant<vm::all_non_recursive>,
        vm_constant<vm::all_recursive>
        >;

    // non-const sm
    mp11::mp_for_each<visit_modes>([&](auto mode_constant)
    {
        constexpr visit_mode mode = decltype(mode_constant)::value;

        upper_machine.template visit<mode>(NoOpConstVisitor{});
        BOOST_REQUIRE(NoOpConstVisitor::copies_or_moves == 0);

        NoOpConstVisitor v0{};
        upper_machine.template visit<mode>(v0);
        BOOST_REQUIRE(NoOpConstVisitor::copies_or_moves == 0);

        const NoOpConstVisitor v0_const{};
        upper_machine.template visit<mode>(v0_const);
        BOOST_REQUIRE(NoOpConstVisitor::copies_or_moves == 0);

        upper_machine.template visit<mode>(NoOpVisitor{});
        BOOST_REQUIRE(NoOpVisitor::copies_or_moves == 0);

        NoOpVisitor v1{};
        upper_machine.template visit<mode>(v1);
        BOOST_REQUIRE(NoOpVisitor::copies_or_moves == 0);

        upper_machine.template visit<mode>([](auto&) {});
    });

    // const sm
    const UpperMachine& upper_machine_c{upper_machine};
    mp11::mp_for_each<visit_modes>([&](auto mode_constant)
    {
        constexpr visit_mode mode = decltype(mode_constant)::value;

        upper_machine_c.template visit<mode>(NoOpConstVisitor{});
        BOOST_REQUIRE(NoOpConstVisitor::copies_or_moves == 0);
        
        NoOpConstVisitor v0{};
        upper_machine_c.template visit<mode>(v0);
        BOOST_REQUIRE(NoOpConstVisitor::copies_or_moves == 0);

        const NoOpConstVisitor v0_c{};
        upper_machine_c.template visit<mode>(v0_c);
        BOOST_REQUIRE(NoOpConstVisitor::copies_or_moves == 0);

        upper_machine_c.template visit<mode>([](auto&) {});
    });
}

} // namespace

using TestMachine = hierarchical_state_machine<favor_compile_time_config>;
BOOST_MSM_BACKMP11_GENERATE_STATE_MACHINE(TestMachine::UpperMachine);
BOOST_MSM_BACKMP11_GENERATE_STATE_MACHINE(TestMachine::MiddleMachine);
BOOST_MSM_BACKMP11_GENERATE_STATE_MACHINE(TestMachine::LowerMachine);

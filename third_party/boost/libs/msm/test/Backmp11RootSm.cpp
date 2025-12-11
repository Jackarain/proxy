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
#include <boost/msm/backmp11/state_machine.hpp>
//front-end
#include <boost/msm/front/state_machine_def.hpp>
#include <boost/msm/front/functor_row.hpp>

#include <boost/msm/back/queue_container_circular.hpp>
#include <boost/msm/back/history_policies.hpp>

#ifndef BOOST_MSM_NONSTANDALONE_TEST
#define BOOST_TEST_MODULE backmp11_root_sm_test
#endif
#include <boost/test/unit_test.hpp>

namespace msm = boost::msm;
namespace mp11 = boost::mp11;

using namespace msm::front;
using namespace msm::backmp11;

namespace
{

// Events.
struct EnterSubFsm{};
struct ExitSubFsm{};
struct TriggerAction{};
struct TriggerActionWithGuard{};

// States.
struct Default : public state<>{};

template <bool TestRootFsmParameter>
struct hierarchical_machine
{
    // Actions
    struct Action
    {
        template<typename Event, typename Fsm, typename Source, typename Target>
        void operator()(const Event&, Fsm&, Source&, Target&)
        {
            if constexpr (TestRootFsmParameter)
            {
                static_assert(std::is_same_v<Fsm,RootSm>);
            }
        }
    };

    // Guards
    struct Guard
    {
        template<typename Event, typename Fsm, typename Source, typename Target>
        bool operator()(const Event&, Fsm&, Source&, Target&)
        {
            if constexpr (TestRootFsmParameter)
            {
                static_assert(std::is_same_v<Fsm,RootSm>);
            }
            return true;
        }
    };

    // Forward-declare the upper machine,
    // so we can set a root_sm.
    struct UpperMachine_;
    struct SmConfig;
    using RootSm = state_machine<UpperMachine_, SmConfig>;
    struct SmConfig : state_machine_config
    {
        using root_sm = RootSm;
        using fsm_parameter = mp11::mp_if_c<TestRootFsmParameter,
            root_sm,
            transition_owner
            >;
    };

    template<typename T>
    struct MachineBase_ : public state_machine_def<MachineBase_<T>>
    {
        template <typename Event, typename Fsm>
        void on_entry(const Event& /*event*/, Fsm& fsm)
        {
            if constexpr (TestRootFsmParameter)
            {
                static_assert(std::is_same_v<Fsm,RootSm>);
            }
            fsm.get_root_sm().machine_entries++;
        };

        template <typename Event, typename Fsm>
        void on_exit(const Event& /*event*/, Fsm& fsm)
        {
            if constexpr (TestRootFsmParameter)
            {
                static_assert(std::is_same_v<Fsm,RootSm>);
            }
            fsm.get_root_sm().machine_exits++;
        };

        using initial_state = Default;
    };

    struct LowerMachine_ : public MachineBase_<LowerMachine_>
    {
        template <typename Event, typename Fsm>
        void on_entry(const Event& /*event*/, Fsm& fsm)
        {
            if constexpr (TestRootFsmParameter)
            {
                static_assert(std::is_same_v<Fsm,RootSm>);
            }
            else
            {
                // TODO:
                // Requires improved filtering of possible entry
                // states.
                // static_assert(std::is_same_v<Fsm,MiddleMachine>);
            }
            fsm.get_root_sm().machine_entries++;
        };

        template <typename Event, typename Fsm>
        void on_exit(const Event& /*event*/, Fsm& fsm)
        {
            if constexpr (TestRootFsmParameter)
            {
                static_assert(std::is_same_v<Fsm,RootSm>);
            }
            else
            {
                // static_assert(std::is_same_v<Fsm,MiddleMachine>);
            }
            fsm.get_root_sm().machine_exits++;
        };
    };
    using LowerMachine = state_machine<LowerMachine_, SmConfig>;

    struct MiddleMachine_ : public MachineBase_<MiddleMachine_>
    {
        using transition_table = mp11::mp_list<
            Row< Default      , TriggerAction          , none         , Action , none  >,
            Row< Default      , TriggerActionWithGuard , none         , Action , Guard >,
            Row< Default      , EnterSubFsm            , LowerMachine                  >,
            Row< LowerMachine , ExitSubFsm             , Default                       >
        >;
    };
    using MiddleMachine = state_machine<MiddleMachine_, SmConfig>;

    struct UpperMachine_ : public MachineBase_<UpperMachine_>
    {
        using transition_table = mp11::mp_list<
            Row< Default       , EnterSubFsm , MiddleMachine>,
            Row< MiddleMachine , ExitSubFsm  , Default>
        >;

        uint32_t machine_entries = 0;
        uint32_t machine_exits = 0;
    };
    using UpperMachine = state_machine<UpperMachine_, SmConfig>;
};

using TestMachines = boost::mpl::vector<
    hierarchical_machine<false>,
    hierarchical_machine<true>
    >;

BOOST_AUTO_TEST_CASE_TEMPLATE( backmp11_root_sm_test, TestMachine, TestMachines )
{
    using UpperMachine = typename TestMachine::UpperMachine;
    UpperMachine upper_machine{};

    upper_machine.start(); 
    BOOST_REQUIRE(upper_machine.machine_entries == 1);

    upper_machine.process_event(EnterSubFsm()); 
    BOOST_REQUIRE(upper_machine.machine_entries == 2);

    upper_machine.process_event(TriggerAction()); 
    upper_machine.process_event(TriggerActionWithGuard()); 

    upper_machine.process_event(EnterSubFsm()); 
    BOOST_REQUIRE(upper_machine.machine_entries == 3);

    upper_machine.process_event(ExitSubFsm()); 
    BOOST_REQUIRE(upper_machine.machine_exits == 1);

    upper_machine.process_event(ExitSubFsm()); 
    BOOST_REQUIRE(upper_machine.machine_exits == 2);

    upper_machine.stop(); 
    BOOST_REQUIRE(upper_machine.machine_exits == 3);
}

} // namespace


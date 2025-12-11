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
#include "Backmp11.hpp"
//front-end
#include <boost/msm/front/state_machine_def.hpp>
#include <boost/msm/front/functor_row.hpp>

#include <boost/msm/back/queue_container_circular.hpp>
#include <boost/msm/back/history_policies.hpp>

#ifndef BOOST_MSM_NONSTANDALONE_TEST
#define BOOST_TEST_MODULE backmp11_context_test
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

// Context.
struct Context
{
    uint32_t machine_entries = 0;
    uint32_t machine_exits = 0;
};
template<typename Fsm>
static Context& get_context(Fsm& fsm)
{
    return fsm.get_context();
}

// States.
struct Default : public state<>{};

template<typename T>
struct MachineBase_ : public state_machine_def<MachineBase_<T>>
{
    template <typename Event, typename Fsm>
    void on_entry(const Event& /*event*/, Fsm& fsm)
    {
        Context& context = get_context(fsm);
        context.machine_entries++;
    };

    template <typename Event, typename Fsm>
    void on_exit(const Event& /*event*/, Fsm& fsm)
    {
        get_context(fsm).machine_exits++;
    };

    using initial_state = Default;
};


template <bool WithRootSm, bool WithFavorCompileTime>
struct Test
{
    class UpperMachine;

    struct Config : default_state_machine_config {
        using context = Context;
        using root_sm = mp11::mp_if_c<WithRootSm, UpperMachine, no_root_sm>;
        using compile_policy = mp11::mp_if_c<WithFavorCompileTime, favor_compile_time, favor_runtime_speed>;
    };

    struct LowerMachine_ : public MachineBase_<LowerMachine_> {};
    using LowerMachine = state_machine<LowerMachine_, Config>;

    struct MiddleMachine_ : public MachineBase_<MiddleMachine_>
    {
        using transition_table = mp11::mp_list<
            Row< Default      , EnterSubFsm , LowerMachine >,
            Row< LowerMachine , ExitSubFsm  , Default      >
        >;
    };
    using MiddleMachine = state_machine<MiddleMachine_, Config>;

    struct UpperMachine_ : public MachineBase_<UpperMachine_>
    {
        UpperMachine_() = default;

        UpperMachine_(bool optional_argument) : optional_argument(optional_argument) {}

        using transition_table = mp11::mp_list<
            Row< Default       , EnterSubFsm , MiddleMachine>,
            Row< MiddleMachine , ExitSubFsm  , Default>
        >;

        bool optional_argument;
    };
    class UpperMachine : public state_machine<UpperMachine_, Config, UpperMachine>
    {
        public:
        using Base = state_machine<UpperMachine_, Config, UpperMachine>;
        using Base::Base;
    };

    static void run()
    {
        // Test with only context.
        {
            Context context;
            UpperMachine test_machine{context};
            BOOST_ASSERT(&test_machine.get_context() == &context);
            auto& middle_machine = test_machine.template get_state<MiddleMachine>();
            BOOST_ASSERT(&middle_machine.get_context() == &context);
            // Suppress warnings in case the assert is not present.
            [[maybe_unused]] auto& lower_machine = middle_machine.template get_state<LowerMachine>();
            BOOST_ASSERT(&lower_machine.get_context() == &context);
            process_events(test_machine);
        }

        // Test with context and additional constructor argument.
        {
            Context context;
            UpperMachine test_machine{context, true};
            BOOST_ASSERT(test_machine.optional_argument == true);
            BOOST_ASSERT(&test_machine.get_context() == &context);
            auto& middle_machine = test_machine.template get_state<MiddleMachine>();
            BOOST_ASSERT(&middle_machine.get_context() == &context);
            // Suppress warnings in case the assert is not present.
            [[maybe_unused]] auto& lower_machine = middle_machine.template get_state<LowerMachine>();
            BOOST_ASSERT(&lower_machine.get_context() == &context);
            BOOST_ASSERT(&lower_machine.get_context() == &context);
            process_events(test_machine);
        }
    };

    static void process_events(UpperMachine& test_machine)
    {
        test_machine.start(); 
        BOOST_CHECK(test_machine.get_context().machine_entries == 1);

        test_machine.process_event(EnterSubFsm()); 
        BOOST_CHECK(test_machine.get_context().machine_entries == 2);

        test_machine.process_event(EnterSubFsm()); 
        BOOST_CHECK(test_machine.get_context().machine_entries == 3);

        test_machine.process_event(ExitSubFsm()); 
        BOOST_CHECK(test_machine.get_context().machine_exits == 1);

        test_machine.process_event(ExitSubFsm()); 
        BOOST_CHECK(test_machine.get_context().machine_exits == 2);

        test_machine.stop(); 
        BOOST_CHECK(test_machine.get_context().machine_exits == 3);
    };
};

BOOST_AUTO_TEST_CASE( backmp11_context_test )
{
    Test<false, false>::run();
    Test<false, true>::run();
    Test<true, false>::run();
    Test<true, true>::run();
}

} // namespace


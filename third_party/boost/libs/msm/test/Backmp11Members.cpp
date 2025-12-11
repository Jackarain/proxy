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
#define BOOST_TEST_MODULE backmp11_members_test
#endif
#include <boost/test/unit_test.hpp>

namespace msm = boost::msm;
namespace mp11 = boost::mp11;

using namespace msm::front;
using namespace msm::backmp11;

namespace
{

// Events.
struct MyEvent{};

// Actions
struct MyAction
{
    template<typename Event, typename Fsm, typename Source, typename Target>
    void operator()(const Event&, Fsm& fsm, Source&, Target&)
    {
        fsm.action_calls++;
    }
};
struct MyGuard
{
    template<typename Event, typename Fsm, typename Source, typename Target>
    bool operator()(const Event&, Fsm& fsm, Source&, Target&) const
    {
        fsm.guard_calls++;
        return true;
    }
};

// States.
struct Default : public state<>{};
class StateMachine;

struct SmConfig : state_machine_config
{
    using root_sm = StateMachine;
};

struct StateMachine_ : public state_machine_def<StateMachine_>
{
    using activate_deferred_events = int;

    template <typename Event, typename Fsm>
    void on_entry(const Event& /*event*/, Fsm& fsm)
    {
        fsm.entry_calls++;
    };

    template <typename Event, typename Fsm>
    void on_exit(const Event& /*event*/, Fsm& fsm)
    {
        fsm.machine_exits++;
    };

    using initial_state = Default;
    using transition_table = mp11::mp_list<
        Row<Default, MyEvent, none, MyAction, MyGuard>
    >;
};
// Leave this class without inheriting constructors to check
// that this only needs to be done in case we use a context
// (for convenience).
class StateMachine : public state_machine<StateMachine_, SmConfig, StateMachine>
{
    public:
    size_t entry_calls{};
    size_t machine_exits{};
    size_t action_calls{};
    size_t guard_calls{};
};

BOOST_AUTO_TEST_CASE( backmp11_members_test )
{
    StateMachine test_machine;
    [[maybe_unused]] auto& events_queue = test_machine.get_events_queue();
    [[maybe_unused]] const auto& const_events_queue = static_cast<const StateMachine*>(&test_machine)->get_events_queue();
    [[maybe_unused]] auto& deferred_events_queue = test_machine.get_deferred_events_queue();
    [[maybe_unused]] const auto& const_deferred_events_queue = static_cast<const StateMachine*>(&test_machine)->get_deferred_events_queue();

    test_machine.start();
    BOOST_CHECK_MESSAGE(test_machine.entry_calls == 1, "SM on_entry not called correctly");
    test_machine.process_event(MyEvent{});
    BOOST_CHECK_MESSAGE(test_machine.action_calls == 1, "SM action not called correctly");
    BOOST_CHECK_MESSAGE(test_machine.guard_calls == 1, "SM guard not called correctly");

    test_machine.stop();
    BOOST_CHECK_MESSAGE(test_machine.machine_exits == 1, "SM on_exit not called correctly");
}

} // namespace

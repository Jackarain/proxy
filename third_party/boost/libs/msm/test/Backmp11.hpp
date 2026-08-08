// Copyright 2025 Christian Granzin
// Copyright 2008 Christophe Henry
// henry UNDERSCORE christophe AT hotmail DOT com
// This is an extended version of the state machine available in the boost::mpl library
// Distributed under the same license as the original.
// Copyright for the original version:
// Copyright 2005 David Abrahams and Aleksey Gurtovoy. Distributed
// under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "boost/msm/backmp11/state_machine.hpp"
#include "boost/msm/backmp11/observer.hpp"
#include "boost/msm/backmp11/favor_compile_time.hpp" // IWYU pragma: keep

#include "Utils.hpp"

namespace boost::msm::backmp11
{

struct favor_compile_time_config : state_machine_config
{
    using compile_policy = favor_compile_time;
};

// Test class for state machines,
// which exposes additional APIs for test assertions.
template <typename FrontEnd, typename Config = default_state_machine_config>
class StateMachine
    : public state_machine<FrontEnd, Config, StateMachine<FrontEnd, Config>>
{
    using base = state_machine<FrontEnd, Config, StateMachine<FrontEnd, Config>>;
  public:
    const typename base::event_pool_container_t& get_pending_events() const
    {
        return this->get_event_pool().events;
    }
};

class TestObserver : public default_observer
{
  public:
    template <typename StateMachine, typename Event>
    void post_process_event(const StateMachine&, const Event& event,
                                      process_result result)
    {
        processed_events += 1;
        if (expect_process_result &&
            expect_process_result.value() != result)
        {
            const auto error = std::string{"\nUnexpected process result for "} +
                               to_string(event) + "\n - expected: " +
                               to_string(expect_process_result.value()) +
                               "\n - actual: " + to_string(result);
            BOOST_FAIL(error);
        }
    }

    template <typename Source, typename Event, typename Target, typename Action,
              typename Guard, typename StateMachine>
    void post_process_transition(const StateMachine& sm, size_t /*region_id*/,
                                 process_result result) const
    {
        assert_post_process_transition<Source, Event, Target, Action, Guard>(
            sm, result);
    }

    template <typename Event, typename Action, typename Guard,
              typename StateMachine>
    void post_process_transition(const StateMachine& sm,
                                           process_result result) const
    {
        assert_post_process_transition<StateMachine, Event, front::none, Action,
                                       Guard>(sm, result);
    }

    size_t processed_events{};
    std::optional<bool> expect_transition;
    std::optional<process_result> expect_transition_result;
    std::optional<process_result> expect_process_result;

  private:
    template <typename Source, typename Event, typename Target, typename Action,
              typename Guard, typename StateMachine>
    void assert_post_process_transition(const StateMachine& sm,
                                        process_result result) const
    {
        const auto transition =
            transition_to_string<Source, Event, Target, Action, Guard>(sm);
        if (expect_transition && !expect_transition.value())
        {
            const auto error =
                std::string{"\nUnexpected transition "} + transition;
            BOOST_FAIL(error);
        }
        if (expect_transition_result &&
            expect_transition_result.value() != result)
        {
            const auto error =
                std::string{"\nUnexpected transition result for "} +
                transition + "\n - expected: " +
                to_string(expect_transition_result.value()) +
                "\n - actual: " + to_string(result);
            BOOST_FAIL(error);
        }
    }
};

} // boost::serialization

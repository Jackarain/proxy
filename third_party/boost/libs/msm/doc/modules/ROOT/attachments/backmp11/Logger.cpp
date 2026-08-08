// Copyright 2026 Christian Granzin
// Copyright 2010 Christophe Henry
// henry UNDERSCORE christophe AT hotmail DOT com
// This is an extended version of the state machine available in the boost::mpl library
// Distributed under the same license as the original.
// Copyright for the original version:
// Copyright 2005 David Abrahams and Aleksey Gurtovoy. Distributed
// under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <string>
#include <iostream>

#include <boost/core/typeinfo.hpp>

#include <boost/msm/backmp11/state_machine.hpp>
#include <boost/msm/backmp11/observer.hpp>
#include <boost/msm/front/functor_row.hpp>
#include <boost/msm/front/state_machine_def.hpp>

namespace back = boost::msm::backmp11;
namespace front = boost::msm::front;
using front::none;
using front::Row;
using front::Internal;
namespace mp11 = boost::mp11;

namespace
{

// Logger for state machine activities.
// Logs the beginning of event processing and the result of each processed transition.
class Logger : public back::default_observer
{
  public:
    template <typename StateMachine, typename Event>
    void pre_process_event(const StateMachine& sm, const Event& event)
    {
        log(to_string(sm) + " processing event " + to_string(event));
    }

    template <typename Source, typename Event, typename Target, typename Action,
              typename Guard, typename StateMachine>
    void post_process_transition(const StateMachine& sm, size_t /*region_id*/,
                                 process_result result)
    {
        log(to_string(sm) + " processed transition \"" + to_string<Source>() +
            " + " + to_string<Event>() + guard_to_string<Guard>() +
            action_to_string<Action>() + target_to_string<Target>() + "\" (" +
            to_string(result) + ")");
    }

    template <typename Event, typename Action, typename Guard,
              typename StateMachine>
    void post_process_transition(const StateMachine& sm, process_result result)
    {
        log(to_string(sm) + " processed transition \"" + to_string(sm) + " + " +
            to_string<Event>() + guard_to_string<Guard>() +
            action_to_string<Action>() + "\" (" + to_string(result) + ")");
    }

  protected:
    virtual void log(std::string_view msg)
    {
        std::cout << msg << std::endl;
    }

  private:
    static std::string to_string(const std::type_info& type)
    {
        auto full_name = boost::core::demangled_name(type);
        return full_name.substr(full_name.rfind(':') + 1);
    }

    template <typename T>
    static std::string to_string()
    {
        return to_string(typeid(T));
    }

    template <typename FrontEnd, typename Config, typename Derived>
    static std::string to_string(const back::state_machine<FrontEnd, Config, Derived>& /*sm*/)
    {
        return to_string<FrontEnd>();
    }

    template <typename T>
    static std::string to_string(const T& /*event*/)
    {
        return to_string<T>();
    }

    static std::string to_string(const std::any& event)
    {
        return to_string(event.type());
    }

    template <typename T>
    static std::string target_to_string()
    {
        if constexpr (!std::is_same_v<T, front::none>)
        {
            return std::string{" -> "} + to_string<T>();
        }
        return {};
    }

    template <typename T>
    static std::string action_to_string()
    {
        if constexpr (!std::is_same_v<T, front::none>)
        {
            return std::string{" / "} + to_string<T>();
        }
        return {};
    }

    template <typename T>
    static std::string guard_to_string()
    {
        if constexpr (!std::is_same_v<T, front::none>)
        {
            return std::string{" [ "} + to_string<T>() + " ]";
        }
        return {};
    }

    static std::string to_string(process_result result)
    {
        switch (result)
        {
            case process_result::discarded:
                return "discarded";
            case process_result::consumed:
                return "consumed";
            case process_result::rejected:
                return "rejected";
            case process_result::deferred:
                return "deferred";
            default:
                return {};
        }
    }
};

// Events
struct TransitionEvent {};
struct InternalTransitionEvent {};
struct SmInternalTransitionEvent {};

// Actions
struct MyAction
{
    template<typename Fsm>
    void operator()(Fsm&)
    {
    }
};

// Guards
struct MyGuard
{
    template<typename Fsm>
    bool operator()(Fsm&) const
    {
        return true;
    }
};

struct NotMyGuard
{
    template<typename Fsm>
    bool operator()(Fsm&) const
    {
        return false;
    }
};

// States.
struct MyState : front::state<> {};
struct MyOtherState : front::state<> {};

// State machine.
// We leave out the trailing underscore for prettier logs.
// You could also implement `to_string()` methods to explicitly set names and
// use these in the logger implementation.
struct MyStateMachine : front::state_machine_def<MyStateMachine>
{
    using initial_state = MyState;
    using transition_table = mp11::mp_list<
        Row<MyState     , InternalTransitionEvent, none        , MyAction, none>,
        Row<MyState     , TransitionEvent        , MyOtherState, MyAction, MyGuard>,
        Row<MyState     , TransitionEvent        , MyOtherState, MyAction, NotMyGuard>,
        Row<MyOtherState, TransitionEvent        , MyState     , none    , none>
    >;
    using internal_transition_table = mp11::mp_list<
        Internal<SmInternalTransitionEvent, MyAction, none>
    >;
};

struct MyConfig : back::state_machine_config
{
    using observer = Logger;
};

using MyStateMachineBe = back::state_machine<MyStateMachine, MyConfig>;

[[maybe_unused]] void logger_example()
{
    MyStateMachineBe test_machine;

    std::cout << "Starting state machine..." << std::endl;
    test_machine.start();

    test_machine.process_event(TransitionEvent{});
    test_machine.process_event(InternalTransitionEvent{});
    test_machine.process_event(TransitionEvent{});
    test_machine.process_event(SmInternalTransitionEvent{});

    std::cout << "Stopping state machine..." << std::endl;
    test_machine.stop();
}

}

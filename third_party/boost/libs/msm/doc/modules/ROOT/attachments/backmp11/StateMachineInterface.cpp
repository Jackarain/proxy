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

#include <boost/core/typeinfo.hpp>
#include <iostream>

#include <boost/msm/backmp11/state_machine.hpp>
#include <boost/msm/backmp11/favor_compile_time.hpp>
#include <boost/msm/front/functor_row.hpp>
#include <boost/msm/front/state_machine_def.hpp>
#include <memory>

namespace back = boost::msm::backmp11;
namespace front = boost::msm::front;
using front::none;
using front::Row;
namespace mp11 = boost::mp11;

namespace
{

// State machine interface
class state_machine
{
  public:
    virtual ~state_machine() = default;

    // Starts the state machine.
    virtual void start() = 0;

    // Stops the state machine.
    virtual void stop() = 0;

    // Checks whether a state is active.
    // Uses a string for state identification to
    // remove the state's type dependency.
    virtual bool is_state_active(std::string_view name) = 0;

    // Processes an event.
    virtual back::process_result process_event(
        const back::any_event& event) = 0;
};

// Implementation of the interface
struct MyConfig : back::state_machine_config
{
    // Use favor compile time, we need type erasure on events
    // for a state machine interface.
    using compile_policy = back::favor_compile_time;
};

template <typename FrontEnd, typename Config = MyConfig>
class state_machine_impl
    : public state_machine,
      public back::state_machine<FrontEnd, Config,
                                 state_machine_impl<FrontEnd, Config>>
{
    using base = back::state_machine<FrontEnd, Config,
                                     state_machine_impl<FrontEnd, Config>>;

  public:
    void start() override
    {
        base::start();
    }

    void stop() override
    {
        base::stop();
    }

    bool is_state_active(std::string_view name) override
    {
        bool result{false};
        this->visit(
        [name, &result](const auto& state)
        {
            if (state.to_string() == name)
            {
                result = true;
            }
        });
        return result;
    }

    back::process_result process_event(const back::any_event& event) override
    {
        return base::process_event(event);
    }
};

// Events
struct Greet {};

// Actions
struct PrintMessage
{
    template <typename Fsm, typename Source, typename Target>
    void operator()(const Greet&, Fsm&, Source& source, Target&)
    {
        if (talk)
        {
            std::cout << "Hello from " << source.to_string() << std::endl;
        }
    }

    [[maybe_unused]] static inline bool talk{true};
};

// States
template <typename Derived>
struct StateBase : front::state<>
{
    static std::string to_string()
    {
        auto full_name = boost::core::demangled_name(typeid(Derived));
        return full_name.substr(full_name.rfind(':') + 1);
    }
};

struct MyState : StateBase<MyState> {};

struct MyOtherState : StateBase<MyOtherState> {};

// State machines

struct MyStateMachine_ : front::state_machine_def<MyStateMachine_>
{
    using initial_state = MyState;
    using transition_table = mp11::mp_list<
        Row<MyState, Greet, none, PrintMessage>
    >;
};

using MyStateMachine = state_machine_impl<MyStateMachine_>;

struct MyOtherStateMachine_ : front::state_machine_def<MyOtherStateMachine_>
{
    using initial_state = MyOtherState;
    using transition_table = mp11::mp_list<
        Row<MyOtherState, Greet, none, PrintMessage>
    >;
};

using MyOtherStateMachine = state_machine_impl<MyOtherStateMachine_>;

[[maybe_unused]] void state_machine_interface_example()
{
    std::unique_ptr<state_machine> state_machine;
    
    state_machine = std::make_unique<MyStateMachine>();
    state_machine->start();
    state_machine->process_event(Greet{});
    std::cout << "Is MyState active: "
              << state_machine->is_state_active("MyState") << std::endl;

    state_machine = std::make_unique<MyOtherStateMachine>();
    state_machine->start();
    state_machine->process_event(Greet{});
    std::cout << "Is MyOtherState active: "
              << state_machine->is_state_active("MyOtherState") << std::endl;
}

} // namespace

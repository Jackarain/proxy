# Meta State Machine (MSM)

MSM is a Boost library that uses C++ metaprogramming for creating high-performance UML2 finite state machines.
It separates state machine modeling (front-end) from execution (back-end), allowing each to be chosen independently.
The classic `back` back-end supports C++03, the `backmp11` back-end requires C++17.

The full documentation is available on Boost ([latest release](https://www.boost.org/doc/libs/latest/doc/antora/msm/index.html), [develop](https://www.boost.org/doc/libs/develop/doc/antora/msm/index.html)).

# Features

Front-end:

- State transitions (external, internal, completion) with actions and guards
- State entry & exit actions
- Deferred events — via state property and transition action
- Kleene (any) events — match any event in a transition
- Event hierarchies — use inheritance to match events in a transition
- Hierarchical state machines (composite states)
- Orthogonal regions
- History
- Pseudostates — entry, exit, fork, terminate
- State flags

Back-end (`backmp11`):

- Shared context in state machine hierarchies — data and root machine reference accessible across all submachines
- Observer — hook into state machine activities for logging and monitoring
- State visitor — traverse through active or all states
- Serialization — built-in support for Boost.Serialization, Boost.JSON, and nlohmann/json
- Compile-time trade-offs — choose between optimizing for runtime speed or compile time
- Heapless execution — configurable to avoid dynamic memory allocations
- Virtual dispatch support — encapsulate implementation details and swap implementations at runtime

# Usage

This example models a connection to a host. It uses the functor front-end and the `backmp11` back-end. More examples are available in the documentation.

```cpp
#include <iostream>

#include <boost/msm/backmp11/state_machine.hpp>
#include <boost/msm/front/functor_row.hpp>
#include <boost/msm/front/state_machine_def.hpp>

namespace mp11 = boost::mp11;
namespace back = boost::msm::backmp11;
namespace front = boost::msm::front;

// Events
struct Connect
{
    std::string host;
};
struct Disconnect {};

// Guards
struct IsValidHost
{
    template <typename Fsm>
    bool operator()(const Connect& event, Fsm&)
    {
        return !event.host.empty();
    }
};

// Actions
struct LogConnection
{
    template <typename Fsm>
    void operator()(const Connect& event, Fsm& fsm)
    {
        std::cout << "Connected to " << event.host
                  << " (connection #" << ++fsm.connection_count << ")"
                  << std::endl;
    }
};

// States
struct Disconnected : front::state<> {};
struct Connected : front::state<> {};

// State machine
struct Connection_ : front::state_machine_def<Connection_>
{
    using initial_state = Disconnected;
    using transition_table = mp11::mp_list<
        //         Source        Event       Target        Action         Guard
        front::Row<Disconnected, Connect,    Connected   , LogConnection, IsValidHost>,
        front::Row<Connected   , Disconnect, Disconnected>
    >;

    size_t connection_count = 0;
};
using Connection = back::state_machine<Connection_>;

int main()
{
    Connection sm;
    sm.start();
    sm.process_event(Connect{""});           // rejected by IsValidHost
    sm.process_event(Connect{"localhost"});  // prints "Connected to localhost (connection #1)"
    sm.process_event(Disconnect{});
}
```

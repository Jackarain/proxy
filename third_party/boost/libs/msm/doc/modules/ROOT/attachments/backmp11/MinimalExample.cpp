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

[[maybe_unused]] void minimal_example()
{
    Connection sm;
    sm.start();
    sm.process_event(Connect{""});           // rejected by IsValidHost
    sm.process_event(Connect{"localhost"});  // prints "Connected to localhost (connection #1)"
    sm.process_event(Disconnect{});
}

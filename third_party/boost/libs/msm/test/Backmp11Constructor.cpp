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
#define BOOST_TEST_MODULE test_constructor
#endif
#include <boost/test/unit_test.hpp>

// back-end
#include "BackCommon.hpp"
#include <boost/msm/backmp11/observer.hpp>
//front-end
#include "FrontCommon.hpp"
#include <boost/config.hpp>

namespace msm = boost::msm;

using namespace msm::front;
using namespace msm::backmp11;


namespace
{

struct MyState : test::StateBase {};

struct StateMachine_ : test::StateMachineBase_<StateMachine_>
{
    using initial_state = MyState;
};

struct Context
{
    int number{};
};

struct context_config : state_machine_config
{
    using context = Context;
};

using Observer = msm::backmp11::default_observer;

struct observer_config : state_machine_config
{
    using observer = Observer;
};

struct co_config : context_config
{
    using observer = Observer;
};


BOOST_AUTO_TEST_CASE(context_constructors)
{
    using StateMachine = state_machine<StateMachine_, context_config>;
    Context context;

    StateMachine sm{context};
}

BOOST_AUTO_TEST_CASE(observer_constructors)
{
    using StateMachine = state_machine<StateMachine_, observer_config>;
    Observer observer;

    StateMachine sm{observer};
}

BOOST_AUTO_TEST_CASE(co_constructors)
{
    using StateMachine = state_machine<StateMachine_, co_config>;
    Context context;
    Observer observer;

    StateMachine sm{context, observer};
}

} // namespace

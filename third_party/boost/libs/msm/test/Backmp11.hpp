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
#include "boost/msm/backmp11/favor_compile_time.hpp"

namespace boost::msm::backmp11
{

struct favor_compile_time_config : state_machine_config
{
    using compile_policy = favor_compile_time;
};


} // boost::serialization
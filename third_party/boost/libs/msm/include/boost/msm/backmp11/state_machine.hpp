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

#ifndef BOOST_MSM_BACKMP11_STATE_MACHINE_HPP
#define BOOST_MSM_BACKMP11_STATE_MACHINE_HPP

#include <boost/msm/backmp11/detail/state_machine_base.hpp>

namespace boost::msm::backmp11
{

/**
 * @brief Back-end for state machines.
 *
 * Can take 1...3 parameters.
 * 
 * @tparam T0 (mandatory) : Front-end
 * @tparam T1 (optional)  : State machine config
 * @tparam T2 (optional)  : Derived class (required when inheriting from state_machine)
 */
template <typename ...T>
class state_machine;

template <class FrontEnd, class Config, class Derived>
class state_machine<FrontEnd, Config, Derived>
    : public detail::state_machine_base<FrontEnd, Config, Derived>
{
    using Base = detail::state_machine_base<FrontEnd, Config, Derived>;
  public:
    using Base::Base;
};

template <class FrontEnd, class Config>
class state_machine<FrontEnd, Config>
    : public detail::state_machine_base<FrontEnd, Config, state_machine<FrontEnd, Config>>
{
    using Base = detail::state_machine_base<FrontEnd, Config, state_machine<FrontEnd, Config>>;
  public:
    using Base::Base;
};

template <class FrontEnd>
class state_machine<FrontEnd>
    : public detail::state_machine_base<FrontEnd, default_state_machine_config, state_machine<FrontEnd>>
{
    using Base = detail::state_machine_base<FrontEnd, default_state_machine_config, state_machine<FrontEnd>>;
  public:
    using Base::Base;
};

} // boost::msm::backmp11

#endif // BOOST_MSM_BACKMP11_STATE_MACHINE_HPP

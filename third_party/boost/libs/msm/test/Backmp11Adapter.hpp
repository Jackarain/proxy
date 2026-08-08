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

#include "attachments/backmp11/BackAdapter.cpp"
#include "Backmp11.hpp"

namespace boost::msm::backmp11
{

template<class, class = void>
struct is_backmp11_state_machine : std::false_type {};

template<class T>
struct is_backmp11_state_machine<T, std::void_t<typename T::config_t>> : std::true_type {};

template <class A0,
          class A1 = parameter::void_,
          class A2 = parameter::void_,
          class A3 = parameter::void_,
          class A4 = parameter::void_>
using state_machine_adapter = back_adapter<A0, A1, A2, A3, A4>;

} // boost::msm::backmp11

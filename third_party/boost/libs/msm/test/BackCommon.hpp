// Copyright 2025 Christian Granzin
// Copyright 2024 Christophe Henry
// henry UNDERSCORE christophe AT hotmail DOT com
// This is an extended version of the state machine available in the boost::mpl library
// Distributed under the same license as the original.
// Copyright for the original version:
// Copyright 2005 David Abrahams and Aleksey Gurtovoy. Distributed
// under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// back-end
#include <boost/msm/back/state_machine.hpp>
#include <boost/msm/back/favor_compile_time.hpp>
#include <boost/msm/back11/state_machine.hpp>

#if (BOOST_CXX_VERSION >= 201703L)
#include <boost/msm/backmp11/state_machine.hpp>
#include <boost/msm/backmp11/favor_compile_time.hpp>
#else
    #define BOOST_MSM_TEST_SKIP_BACKMP11
#endif // BOOST_CXX_VERSION

#ifdef BOOST_MSM_TEST_ONLY_BACKMP11
    #define BOOST_MSM_TEST_MAYBE_COMMA
#else
    #define BOOST_MSM_TEST_MAYBE_COMMA ,
#endif // BOOST_MSM_TEST_ONLY_BACKMP11

#ifndef BOOST_MSM_TEST_SKIP_BACKMP11
#include "Backmp11Adapter.hpp"
#endif // BOOST_MSM_TEST_SKIP_BACKMP11

template<typename Front>
using get_test_machines = boost::mpl::vector<
#if !defined(BOOST_MSM_TEST_SKIP_BACKMP11)
    boost::msm::backmp11::state_machine_adapter<Front>,
    boost::msm::backmp11::state_machine_adapter<Front, boost::msm::backmp11::favor_compile_time>
    BOOST_MSM_TEST_MAYBE_COMMA
#endif // BOOST_MSM_TEST_SKIP_BACKMP11
#if !defined(BOOST_MSM_TEST_ONLY_BACKMP11)
    boost::msm::back::state_machine<Front>,
    boost::msm::back::state_machine<Front, boost::msm::back::favor_compile_time>,
    boost::msm::back11::state_machine<Front>
#endif // BOOST_MSM_TEST_ONLY_BACKMP11
    >;

template <template <template <typename...> class, typename = void> class hierarchical>
using get_hierarchical_test_machines = boost::mpl::vector<
#if !defined(BOOST_MSM_TEST_SKIP_BACKMP11)
    hierarchical<boost::msm::backmp11::state_machine_adapter>,
    hierarchical<boost::msm::backmp11::state_machine_adapter, boost::msm::backmp11::favor_compile_time>
    BOOST_MSM_TEST_MAYBE_COMMA
#endif // BOOST_MSM_TEST_SKIP_BACKMP11
#if !defined(BOOST_MSM_TEST_ONLY_BACKMP11)
    hierarchical<boost::msm::back::state_machine>,
    hierarchical<boost::msm::back::state_machine, boost::msm::back::favor_compile_time>,
    hierarchical<boost::msm::back11::state_machine>
#endif // BOOST_MSM_TEST_ONLY_BACKMP11
>;

#define BOOST_MSM_TEST_DEFINE_DEPENDENT_TEMPLATES(frontname)                          \
    using base = boost::msm::front::state_machine_def<frontname>;                     \
    template<typename T1, class Event, typename T2>                                   \
    using _row = typename base::template _row<T1, Event, T2>;                         \
    template<                                                                         \
        typename T1,                                                                  \
        class Event,                                                                  \
        typename T2,                                                                  \
        void (frontname::*action)(Event const&),                                      \
        bool (frontname::*guard)(Event const&)                                        \
        > using row = typename base::template row<T1, Event, T2, action, guard>;      \
    template<                                                                         \
        typename T1,                                                                  \
        class Event,                                                                  \
        typename T2,                                                                  \
        void (frontname::*action)(Event const&)                                       \
        > using a_row = typename base::template a_row<T1, Event, T2, action>;         \
    template<                                                                         \
        typename T1,                                                                  \
        class Event,                                                                  \
        typename T2,                                                                  \
        bool (frontname::*guard)(Event const&)                                        \
    > using g_row = typename base::template g_row<T1, Event, T2, guard>;

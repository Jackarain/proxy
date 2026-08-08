/*-----------------------------------------------------------------------------+
Copyright (c) 2026: Joachim Faulhaber
+------------------------------------------------------------------------------+
   Distributed under the Boost Software License, Version 1.0.
      (See accompanying file LICENCE.txt or copy at
           http://www.boost.org/LICENSE_1_0.txt)
+-----------------------------------------------------------------------------*/
#define BOOST_TEST_MODULE icl::issues unit test

#include <disable_test_warnings.hpp>
#include "../unit_test_unwarned.hpp"

#include <boost/icl/interval_set.hpp>
#include <boost/icl/right_open_interval.hpp>


//CL #include <iostream>

using namespace std;
using namespace boost;
using namespace unit_test;
using namespace boost::icl;

using right_open_interval_set = interval_set<int, std::less, right_open_interval<int>>; 
using const_interval_iterator = right_open_interval_set::const_iterator;

// 
// https://github.com/boostorg/icl/issues/51
BOOST_AUTO_TEST_CASE(issue_51_exclusive_less_not_std_compliant)
{
    right_open_interval_set itvset;

    itvset.add(right_open_interval<int>(0, 2));
    itvset.add(right_open_interval<int>(3, 5));
    itvset.add(right_open_interval<int>(6, 9));
    itvset.add(right_open_interval<int>(10, 12));

    auto ro_itv_4_7 = right_open_interval<int>(4, 7);
    auto lwb = itvset.lower_bound(right_open_interval<int>(4, 7));
    auto upb = itvset.upper_bound(ro_itv_4_7);
    BOOST_CHECK_EQUAL(*lwb, right_open_interval<int>(3, 5));
    BOOST_CHECK_EQUAL(*upb, right_open_interval<int>(10, 12));

    auto collision = itvset.equal_range(ro_itv_4_7);
    // equal_range returns the range of intervals in itvset that collide with interval [4,7)
    BOOST_CHECK_EQUAL(*collision.first,  *lwb);
    BOOST_CHECK_EQUAL(*collision.second, *upb);

    const_interval_iterator cit = lwb;

    BOOST_CHECK_EQUAL(*cit, right_open_interval<int>(3, 5));
    BOOST_CHECK(intersects(*cit, ro_itv_4_7));
    auto intersect1 = *cit & ro_itv_4_7;
    BOOST_CHECK_EQUAL(4, last(intersect1));

    ++cit;
    BOOST_CHECK_EQUAL(*cit, right_open_interval<int>(6, 9));
    auto intersect2 = *cit & ro_itv_4_7;
    BOOST_CHECK_EQUAL(6, last(intersect2));

    ++cit;
    auto ro_itv_10_12 = right_open_interval<int>(10, 12);
    BOOST_CHECK_EQUAL(*cit, ro_itv_10_12);
    BOOST_CHECK_EQUAL(*upb, ro_itv_10_12);
    BOOST_CHECK( exclusive_less(ro_itv_4_7, ro_itv_10_12) );
    BOOST_CHECK( disjoint(ro_itv_4_7, ro_itv_10_12) );
}

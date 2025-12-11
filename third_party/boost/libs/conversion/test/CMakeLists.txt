# Copyright  (c) 2016-2025 Antony Polukhin
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

include(BoostTest OPTIONAL RESULT_VARIABLE HAVE_BOOST_TEST)

if(NOT HAVE_BOOST_TEST)
  return()
endif()

set(BOOST_TEST_LINK_LIBRARIES Boost::conversion Boost::core)

boost_test(TYPE run SOURCES cast_test.cpp)
boost_test(TYPE run SOURCES implicit_cast.cpp)
boost_test(TYPE compile-fail SOURCES implicit_cast_fail.cpp)
boost_test(TYPE compile-fail SOURCES implicit_cast_fail2.cpp)
boost_test(TYPE run SOURCES polymorphic_cast_test.cpp LINK_LIBRARIES Boost::smart_ptr)

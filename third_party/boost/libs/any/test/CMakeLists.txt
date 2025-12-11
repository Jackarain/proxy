# Copyright  (c) 2016-2025 Antony Polukhin
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

include(BoostTest OPTIONAL RESULT_VARIABLE HAVE_BOOST_TEST)

if(NOT HAVE_BOOST_TEST)
  return()
endif()

set(BOOST_TEST_LINK_LIBRARIES Boost::any Boost::core)

boost_test(TYPE run SOURCES any_test.cpp)
boost_test(TYPE run SOURCES any_test.cpp COMPILE_OPTIONS "-fno-rtti" NAME any_test_no_rtti )

boost_test(TYPE run SOURCES any_test_rv.cpp)
boost_test(TYPE run SOURCES any_test_rv.cpp COMPILE_OPTIONS "-fno-rtti" NAME any_test_rv_no_rtti)
boost_test(TYPE run SOURCES any_test_mplif.cpp)
boost_test(TYPE compile-fail SOURCES any_cast_cv_failed.cpp)
boost_test(TYPE compile-fail SOURCES any_test_temporary_to_ref_failed.cpp)
boost_test(TYPE compile-fail SOURCES any_test_cv_to_rv_failed.cpp)
boost_test(TYPE run SOURCES basic_any_test.cpp)
boost_test(TYPE run SOURCES basic_any_test.cpp COMPILE_OPTIONS "-fno-rtti" NAME basic_any_test_no_rtti)
boost_test(TYPE run SOURCES basic_any_test_rv.cpp)
boost_test(TYPE run SOURCES basic_any_test_rv.cpp COMPILE_OPTIONS "-fno-rtti" NAME basic_any_test_rv_no_rtti)
boost_test(TYPE run SOURCES basic_any_test_mplif.cpp)
boost_test(TYPE run SOURCES basic_any_test_large_object.cpp)
boost_test(TYPE run SOURCES basic_any_test_small_object.cpp)
boost_test(TYPE compile-fail SOURCES basic_any_cast_cv_failed.cpp)
boost_test(TYPE compile-fail SOURCES basic_any_test_alignment_power_of_two_failed.cpp)
boost_test(TYPE compile-fail SOURCES basic_any_test_cv_to_rv_failed.cpp)
boost_test(TYPE compile-fail SOURCES basic_any_test_multiply_size_failed.cpp)
boost_test(TYPE compile-fail SOURCES basic_any_test_size_alignment_zero_failed.cpp)
boost_test(TYPE compile-fail SOURCES basic_any_test_size_less_alignment_failed.cpp)
boost_test(TYPE compile-fail SOURCES basic_any_test_temporary_to_ref_failed.cpp)

boost_test(TYPE compile-fail SOURCES any_from_basic_any.cpp)
boost_test(TYPE compile-fail SOURCES any_to_basic_any.cpp)
boost_test(TYPE compile-fail SOURCES basic_any_from_any.cpp)
boost_test(TYPE compile-fail SOURCES basic_any_to_any.cpp)
boost_test(TYPE compile-fail SOURCES any_from_basic_any_rv.cpp)
boost_test(TYPE compile-fail SOURCES any_to_basic_any_rv.cpp)
boost_test(TYPE compile-fail SOURCES basic_any_from_any_rv.cpp)
boost_test(TYPE compile-fail SOURCES basic_any_to_any_rv.cpp)

boost_test(TYPE compile-fail SOURCES basic_any_from_basic_any.cpp)
boost_test(TYPE compile-fail SOURCES basic_any_to_basic_any.cpp)
boost_test(TYPE compile-fail SOURCES basic_any_from_basic_any_rv.cpp)
boost_test(TYPE compile-fail SOURCES basic_any_to_basic_any_rv.cpp)

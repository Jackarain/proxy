# Copyright 2024 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

include(BoostTestJamfile OPTIONAL RESULT_VARIABLE HAVE_BOOST_TEST)

if(NOT HAVE_BOOST_TEST)
  return()
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

boost_test_jamfile(FILE Jamfile.v2 LINK_LIBRARIES Boost::uuid Boost::core Boost::container_hash Boost::lexical_cast Boost::predef Boost::array)

set(BOOST_TEST_LINK_LIBRARIES Boost::uuid Boost::core)

boost_test(TYPE run SOURCES test_uuid.cpp LINK_LIBRARIES Boost::container_hash COMPILE_DEFINITIONS BOOST_UUID_NO_SIMD=1 NAME test_uuid_no_simd)

boost_test(TYPE run SOURCES test_comparison.cpp COMPILE_DEFINITIONS BOOST_UUID_NO_SIMD=1 NAME test_comparison_no_simd)

boost_test(TYPE run SOURCES test_io.cpp LINK_LIBRARIES Boost::lexical_cast Boost::predef COMPILE_DEFINITIONS BOOST_UUID_NO_SIMD=1 NAME test_io_no_simd)
boost_test(TYPE run SOURCES test_io_2.cpp COMPILE_DEFINITIONS BOOST_UUID_NO_SIMD=1 NAME test_io_2_no_simd)

boost_test(TYPE run SOURCES test_to_chars.cpp COMPILE_DEFINITIONS BOOST_UUID_NO_SIMD=1 NAME test_to_chars_no_simd)
boost_test(TYPE run SOURCES test_to_chars_2.cpp COMPILE_DEFINITIONS BOOST_UUID_NO_SIMD=1 NAME test_to_chars_2_no_simd)

boost_test(TYPE run SOURCES test_from_chars.cpp COMPILE_DEFINITIONS BOOST_UUID_NO_SIMD=1 NAME test_from_chars_no_simd)
boost_test(TYPE run SOURCES test_from_chars_2.cpp COMPILE_DEFINITIONS BOOST_UUID_NO_SIMD=1 NAME test_from_chars_2_no_simd)

boost_test(TYPE run SOURCES test_random_generator.cpp LINK_LIBRARIES Boost::random Boost::predef Boost::move)

boost_test(TYPE run SOURCES test_time_generator_v1_3.cpp LINK_LIBRARIES Threads::Threads)

boost_test(TYPE run SOURCES test_time_generator_v6_3.cpp LINK_LIBRARIES Threads::Threads)

boost_test(TYPE run SOURCES test_boost_unordered.cpp LINK_LIBRARIES Boost::unordered)

boost_test(TYPE run SOURCES test_serialization.cpp LINK_LIBRARIES Boost::serialization)

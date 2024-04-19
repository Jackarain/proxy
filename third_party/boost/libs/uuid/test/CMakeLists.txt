# Copyright 2024 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

include(BoostTest OPTIONAL RESULT_VARIABLE HAVE_BOOST_TEST)

include(BoostTest OPTIONAL RESULT_VARIABLE HAVE_BOOST_TEST)

if(NOT HAVE_BOOST_TEST)
  return()
endif()

set(BOOST_TEST_LINK_LIBRARIES Boost::uuid Boost::core)

boost_test(TYPE run SOURCES test_uuid.cpp)
boost_test(TYPE run SOURCES test_uuid_no_simd.cpp)

boost_test(TYPE run SOURCES test_io.cpp LINK_LIBRARIES Boost::lexical_cast)

boost_test(TYPE run SOURCES test_nil_generator.cpp)
boost_test(TYPE run SOURCES test_name_generator.cpp)
boost_test(TYPE run SOURCES test_string_generator.cpp)
boost_test(TYPE run SOURCES test_random_generator.cpp)

boost_test(TYPE run SOURCES test_tagging.cpp)

boost_test(TYPE run SOURCES test_uuid_class.cpp)
boost_test(TYPE run SOURCES test_uuid_in_map.cpp)

boost_test(TYPE run SOURCES test_hash.cpp)
boost_test(TYPE run SOURCES test_md5.cpp)
boost_test(TYPE run SOURCES test_sha1.cpp)

boost_test(TYPE run SOURCES test_entropy_error.cpp)

boost_test(TYPE run SOURCES test_detail_random_provider.cpp)

boost_test(TYPE run SOURCES test_serialization.cpp LINK_LIBRARIES Boost::serialization)

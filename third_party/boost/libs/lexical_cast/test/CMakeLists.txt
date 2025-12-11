# Copyright  (c) 2016-2025 Antony Polukhin
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

include(BoostTest OPTIONAL RESULT_VARIABLE HAVE_BOOST_TEST)

if(NOT HAVE_BOOST_TEST)
  return()
endif()

set(BOOST_TEST_LINK_LIBRARIES Boost::lexical_cast Boost::core)

boost_test(TYPE run SOURCES lexical_cast_test.cpp LINK_LIBRARIES Boost::type_traits)
boost_test(TYPE run SOURCES loopback_test.cpp)
boost_test(TYPE run SOURCES abstract_test.cpp)
boost_test(TYPE run SOURCES noncopyable_test.cpp)
boost_test(TYPE run SOURCES vc8_bug_test.cpp)
boost_test(TYPE run SOURCES implicit_convert.cpp)
boost_test(TYPE run SOURCES wchars_test.cpp)
boost_test(TYPE run SOURCES float_types_test.cpp)

if(NOT MSVC)
    # Make sure that LexicalCast works the same way as some of the C++ Standard Libraries
    boost_test(TYPE run SOURCES float_types_test.cpp COMPILE_DEFINITIONS BOOST_LEXICAL_CAST_DETAIL_TEST_ON_OLD NAME float_types_non_opt)
endif()

boost_test(TYPE run SOURCES inf_nan_test.cpp)
boost_test(TYPE run SOURCES containers_test.cpp)
boost_test(TYPE run SOURCES empty_input_test.cpp LINK_LIBRARIES Boost::range)
boost_test(TYPE run SOURCES pointers_test.cpp)

if(MSVC)
    boost_test(TYPE compile SOURCES typedefed_wchar_test.cpp COMPILE_OPTIONS "/Zc:wchar_t-")
    boost_test(TYPE run SOURCES typedefed_wchar_test_runtime.cpp COMPILE_OPTIONS "/Zc:wchar_t-")
endif()

boost_test(TYPE run SOURCES no_locale_test.cpp
    COMPILE_DEFINITIONS BOOST_NO_STD_LOCALE BOOST_LEXICAL_CAST_ASSUME_C_LOCALE
    LINK_LIBRARIES Boost::range
)

boost_test(TYPE run SOURCES no_exceptions_test.cpp
    COMPILE_OPTIONS "-fno-exceptions"
    COMPILE_DEFINITIONS
        _HAS_EXCEPTIONS=0 # MSVC stdlib
        _STLP_NO_EXCEPTIONS # STLPort
    LINK_LIBRARIES Boost::range
)

boost_test(TYPE run SOURCES iterator_range_test.cpp LINK_LIBRARIES Boost::range)
boost_test(TYPE run SOURCES string_view_test.cpp LINK_LIBRARIES Boost::utility)
boost_test(TYPE run SOURCES arrays_test.cpp LINK_LIBRARIES Boost::array)
boost_test(TYPE run SOURCES integral_types_test.cpp LINK_LIBRARIES Boost::type_traits)
boost_test(TYPE run SOURCES stream_detection_test.cpp)
boost_test(TYPE run SOURCES stream_traits_test.cpp LINK_LIBRARIES Boost::array Boost::range Boost::utility)

boost_test(TYPE compile-fail SOURCES to_pointer_test.cpp)
boost_test(TYPE compile-fail SOURCES from_volatile.cpp)
boost_test(TYPE run SOURCES filesystem_test.cpp LINK_LIBRARIES Boost::filesystem)
boost_test(TYPE run SOURCES try_lexical_convert.cpp)

file(GLOB EXAMPLE_FILES "../example/*.cpp")
foreach (testsourcefile ${EXAMPLE_FILES})
    boost_test(TYPE run SOURCES ${testsourcefile} LINK_LIBRARIES Boost::variant Boost::date_time)
endforeach()

# Copyright Antony Polukhin, 2021-2026.
#
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

if(NOT TARGET tests)
  add_custom_target(tests)
endif()

function(boost_lexical_cast_add_test_impl test_name test_file libs)
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} PRIVATE ${libs})
    add_test(NAME ${test_name} COMMAND ${test_name})
    add_dependencies(tests ${test_name})
endfunction()

function(boost_lexical_cast_add_test name libs)
    set(test_name lexical_cast_${name})
    set(test_file ${name}.cpp)
    boost_lexical_cast_add_test_impl(${test_name} ${test_file} "Boost::lexical_cast;${libs}")
endfunction()

boost_lexical_cast_add_test(lexical_cast_test Boost::type_traits)
boost_lexical_cast_add_test(loopback_test "")
boost_lexical_cast_add_test(abstract_test "")
boost_lexical_cast_add_test(noncopyable_test "")
boost_lexical_cast_add_test(vc8_bug_test "")
boost_lexical_cast_add_test(implicit_convert "")
boost_lexical_cast_add_test(wchars_test "")
boost_lexical_cast_add_test(float_types_test "")

if(NOT MSVC)
    # Make sure that LexicalCast works the same way as some of the C++ Standard Libraries
    boost_lexical_cast_add_test_impl(lexical_cast_float_types_non_opt float_types_test.cpp Boost::lexical_cast)
    target_compile_definitions(lexical_cast_float_types_non_opt PRIVATE BOOST_LEXICAL_CAST_DETAIL_TEST_ON_OLD)
endif()

boost_lexical_cast_add_test(inf_nan_test "")
boost_lexical_cast_add_test(containers_test "")
boost_lexical_cast_add_test(empty_input_test Boost::range)
boost_lexical_cast_add_test(pointers_test "")

if(MSVC)
    add_executable(lexical_cast_typedefed_wchar_test_compile typedefed_wchar_test.cpp)
    target_compile_options(lexical_cast_typedefed_wchar_test_compile PRIVATE "/Zc:wchar_t-")

    boost_lexical_cast_add_test(typedefed_wchar_test_runtime "")
    target_compile_options(lexical_cast_typedefed_wchar_test_runtime PRIVATE "/Zc:wchar_t-")
endif()

boost_lexical_cast_add_test(no_locale_test Boost::range)
target_compile_definitions(lexical_cast_no_locale_test
    PRIVATE
      BOOST_NO_STD_LOCALE
      BOOST_LEXICAL_CAST_ASSUME_C_LOCALE
)

boost_lexical_cast_add_test_impl(lexical_cast_no_exceptions_test no_exceptions_test.cpp "Boost::container;Boost::range")
target_include_directories(lexical_cast_no_exceptions_test PRIVATE ../include)
target_compile_definitions(lexical_cast_no_exceptions_test
    PRIVATE
      _HAS_EXCEPTIONS=0 # MSVC stdlib
      _STLP_NO_EXCEPTIONS # STLPort
)
if(MSVC)
    target_compile_definitions(lexical_cast_no_exceptions_test PRIVATE BOOST_NO_EXCEPTIONS)
endif()
target_compile_options(lexical_cast_no_exceptions_test PRIVATE "-fno-exceptions")

boost_lexical_cast_add_test(iterator_range_test Boost::range)
boost_lexical_cast_add_test(string_view_test Boost::utility)
boost_lexical_cast_add_test(arrays_test Boost::array)
boost_lexical_cast_add_test(integral_types_test Boost::type_traits)
boost_lexical_cast_add_test(stream_detection_test "")
boost_lexical_cast_add_test(stream_traits_test "Boost::array;Boost::range;Boost::utility")

boost_lexical_cast_add_test(filesystem_test Boost::filesystem)
boost_lexical_cast_add_test(try_lexical_convert "")

file(GLOB EXAMPLE_FILES "../example/*.cpp")
foreach (testsourcefile ${EXAMPLE_FILES})
    get_filename_component(testname ${testsourcefile} NAME_WE)
    boost_lexical_cast_add_test_impl(lexical_cast_example_${testname} ${testsourcefile} "Boost::variant;Boost::date_time")
endforeach()

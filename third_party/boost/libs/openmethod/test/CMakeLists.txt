#
# Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
# Copyright (c) 2021 DMitry Arkhipov (grisumbras@gmail.com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/openmethod
#

message(STATUS "Boost.OpenMethod: building tests")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(BOOST_OPENMETHOD_ENABLE_RUNTIME_CHECKS)
endif()

# Custom target used by the boost super-project
if (NOT TARGET tests)
    add_custom_target(tests)
    set_property(TARGET tests PROPERTY FOLDER Dependencies)
endif()

# Replicate error flags from Jamfile
if (BOOST_OPENMETHOD_WARNINGS_AS_ERRORS)
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 7)
            set(BOOST_OPENMETHOD_TEST_FLAGS "-Wall -Werror -Wno-unused-but-set-variable -Wno-maybe-uninitialized")
        else()
            set(BOOST_OPENMETHOD_TEST_FLAGS "-Wall -Werror -Wno-unused-but-set-variable")
        endif()
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
        set(BOOST_OPENMETHOD_TEST_FLAGS "-Wall -Werror")
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT MATCHES "MSVC")
        if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 13)
            set(BOOST_OPENMETHOD_TEST_FLAGS "-Wall -Werror -Wno-unused-but-set-variable")
        else()
            set(BOOST_OPENMETHOD_TEST_FLAGS "-Wall -Werror")
        endif()
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT MATCHES "MSVC")
        set(BOOST_OPENMETHOD_TEST_FLAGS "/W4 /WX /we4265 /wd4251")
    endif()

    # Print test configuration if running in CI
    # This is useful for debugging CI failures related to warnings which might be false positives
    if (DEFINED ENV{CI})
        message(STATUS "Boost.OpenMethod Tests - Compiler ID: ${CMAKE_CXX_COMPILER_ID} / ${CMAKE_CXX_COMPILER_VERSION}")
        if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT)
            message(STATUS "Boost.OpenMethod Tests - Compiler Frontend: ${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}")
        endif()
        message(STATUS "Boost.OpenMethod Tests - Platform: ${CMAKE_SYSTEM_NAME} / ${CMAKE_SYSTEM_VERSION}")
        message(STATUS "Boost.OpenMethod Tests - C++ standard: ${CMAKE_CXX_STANDARD}")
        message(STATUS "Boost.OpenMethod Tests - Test error flags: ${BOOST_OPENMETHOD_TEST_FLAGS}")
    endif()
endif()

file(GLOB test_cpp_files "test_*.cpp")

foreach(test_cpp ${test_cpp_files})
    get_filename_component(test ${test_cpp} NAME_WE)
    set(test_target "boost_openmethod-${test}")
    add_executable(${test_target} EXCLUDE_FROM_ALL ${test_cpp})
    target_link_libraries(${test_target} PRIVATE Boost::openmethod Boost::unit_test_framework)
    add_test(NAME ${test_target} COMMAND ${test_target})
    add_dependencies(tests ${test_target})
endforeach()

add_executable(boost_openmethod-test_mix_release_debug EXCLUDE_FROM_ALL mix_release_debug/main.cpp mix_release_debug/lib.cpp)
target_link_libraries(boost_openmethod-test_mix_release_debug PRIVATE Boost::openmethod Boost::unit_test_framework)
add_test(NAME boost_openmethod-test_mix_release_debug COMMAND boost_openmethod-test_mix_release_debug)
add_dependencies(tests boost_openmethod-test_mix_release_debug)

function(openmethod_compile_fail_test testname fail_regex)
    set(test_target "boost_openmethod-${testname}")
    add_library(${test_target} STATIC EXCLUDE_FROM_ALL "${testname}.cpp")
    target_link_libraries(${test_target} PRIVATE Boost::openmethod)
    add_test(
        NAME "${test_target}"
        COMMAND "${CMAKE_COMMAND}" --build ${CMAKE_BINARY_DIR} --target "${test_target}" --config $<CONFIG>)
    set_property(TEST "${test_target}" PROPERTY PASS_REGULAR_EXPRESSION "${fail_regex}")
endfunction()

openmethod_compile_fail_test(
    compile_fail_non_polymorphic_virtual_parameter "parameter is not a polymorphic class")
openmethod_compile_fail_test(
    compile_fail_non_polymorphic_virtual_ptr ".*")
openmethod_compile_fail_test(
    compile_fail_virtual_parameter_to_value "virtual_traits not specialized for type")
openmethod_compile_fail_test(
    compile_fail_virtual_ptr_different_registries "registry mismatch")
openmethod_compile_fail_test(
    compile_fail_virtual_ptr_other
    "virtual_ptr<> is required in overrider in same position as in method")
openmethod_compile_fail_test(
    compile_fail_virtual_ptr_ref_to_value "different virtual_ptr<> reference categories")
openmethod_compile_fail_test(
    compile_fail_virtual_ptr_shared_not_const "std::shared_ptr cannot be passed by non-const lvalue reference")
openmethod_compile_fail_test(
    compile_fail_virtual_ptr_value_to_ref "different virtual_ptr<> reference categories")
openmethod_compile_fail_test(
    compile_fail_virtual_parameter_private_base_macros "error")
openmethod_compile_fail_test(
    compile_fail_virtual_parameter_private_base_core "must be an unambiguous accessible base")
openmethod_compile_fail_test(
    compile_fail_repeated_inheritance "repeated inheritance")

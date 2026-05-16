# Copyright (c) 2018-2024 Jean-Louis Leroy
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt
# or copy at http://www.boost.org/LICENSE_1_0.txt)

message(STATUS "Boost.OpenMethod: building shared library examples")

add_compile_definitions(BOOST_OPENMETHOD_ENABLE_RUNTIME_CHECKS)

# All targets output to the same directory so that executables can locate shared
# libraries via boost::dll::program_location().parent_path().
set(shared_libs_output_dir "${CMAKE_CURRENT_BINARY_DIR}")

# Helper: add a CTest fixture that builds a CMake target, so that `ctest` alone
# (without a prior `cmake --build`) still works.
function(openmethod_shared_libs_build_fixture target)
    add_test(NAME ${target}-build
        COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}"
                --target ${target} --config $<CONFIG>)
    set_tests_properties(${target}-build PROPERTIES FIXTURES_SETUP ${target}-fixture)
endfunction()

# ------------------------------------------------------------------------------
# static linking

add_library(boost_openmethod-shared SHARED extensions.cpp)
target_link_libraries(boost_openmethod-shared Boost::openmethod)
set_target_properties(boost_openmethod-shared PROPERTIES
    ENABLE_EXPORTS ON
    OUTPUT_NAME shared
    LIBRARY_OUTPUT_DIRECTORY "${shared_libs_output_dir}"
    RUNTIME_OUTPUT_DIRECTORY "${shared_libs_output_dir}"
)

add_executable(boost_openmethod-static static_main.cpp)
target_link_libraries(boost_openmethod-static Boost::openmethod Boost::dll boost_openmethod-shared)
set_target_properties(boost_openmethod-static PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${shared_libs_output_dir}"
)
openmethod_shared_libs_build_fixture(boost_openmethod-static)
add_test(NAME boost_openmethod-static
    COMMAND "${shared_libs_output_dir}/boost_openmethod-static")
set_tests_properties(boost_openmethod-static PROPERTIES
    FIXTURES_REQUIRED boost_openmethod-static-fixture)

# ------------------------------------------------------------------------------
# dynamic loading, direct virtual_ptrs

add_executable(boost_openmethod-dynamic dynamic_main.cpp)
set_target_properties(boost_openmethod-dynamic PROPERTIES
    ENABLE_EXPORTS ON
    RUNTIME_OUTPUT_DIRECTORY "${shared_libs_output_dir}"
)
target_link_libraries(boost_openmethod-dynamic Boost::openmethod Boost::dll)
add_dependencies(boost_openmethod-dynamic boost_openmethod-shared)
if (NOT WIN32)
    openmethod_shared_libs_build_fixture(boost_openmethod-shared)
    openmethod_shared_libs_build_fixture(boost_openmethod-dynamic)
    add_test(NAME boost_openmethod-dynamic
        COMMAND "${shared_libs_output_dir}/boost_openmethod-dynamic")
    set_tests_properties(boost_openmethod-dynamic PROPERTIES
        FIXTURES_REQUIRED "boost_openmethod-shared-fixture;boost_openmethod-dynamic-fixture")
endif()

# ------------------------------------------------------------------------------
# dynamic loading, indirect virtual_ptrs

add_library(boost_openmethod-indirect_shared SHARED indirect_extensions.cpp)
target_compile_definitions(
    boost_openmethod-indirect_shared PUBLIC BOOST_OPENMETHOD_DEFAULT_REGISTRY=indirect_registry)
target_link_libraries(boost_openmethod-indirect_shared PRIVATE Boost::openmethod Boost::dll)
set_target_properties(boost_openmethod-indirect_shared PROPERTIES
    ENABLE_EXPORTS ON
    OUTPUT_NAME indirect_shared
    LIBRARY_OUTPUT_DIRECTORY "${shared_libs_output_dir}"
    RUNTIME_OUTPUT_DIRECTORY "${shared_libs_output_dir}"
)

add_executable(boost_openmethod-indirect indirect_main.cpp)
target_compile_definitions(
    boost_openmethod-indirect PUBLIC BOOST_OPENMETHOD_DEFAULT_REGISTRY=indirect_registry)
set_target_properties(boost_openmethod-indirect PROPERTIES
    ENABLE_EXPORTS ON
    RUNTIME_OUTPUT_DIRECTORY "${shared_libs_output_dir}"
)
target_link_libraries(boost_openmethod-indirect PRIVATE Boost::openmethod Boost::dll)
add_dependencies(boost_openmethod-indirect boost_openmethod-indirect_shared)
if (NOT WIN32)
    openmethod_shared_libs_build_fixture(boost_openmethod-indirect)
    add_test(NAME boost_openmethod-indirect
        COMMAND "${shared_libs_output_dir}/boost_openmethod-indirect")
    set_tests_properties(boost_openmethod-indirect PROPERTIES
        FIXTURES_REQUIRED boost_openmethod-indirect-fixture)
endif()

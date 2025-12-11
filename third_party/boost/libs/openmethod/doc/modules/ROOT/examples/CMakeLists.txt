# Copyright (c) 2018-2024 Jean-Louis Leroy
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt
# or copy at http://www.boost.org/LICENSE_1_0.txt)

message(STATUS "Boost.OpenMethod: building examples")

if (CMAKE_BUILD_TYPE STREQUAL "Release")
  if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-save-temps=obj -masm=intel)
  endif()
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(BOOST_OPENMETHOD_ENABLE_RUNTIME_CHECKS)
endif()

file(GLOB cpp_files "*.cpp")

foreach (cpp ${cpp_files})
    get_filename_component(stem ${cpp} NAME_WE)
    set(test_target "boost_openmethod-${stem}")
    add_executable(${test_target} ${cpp})
    target_link_libraries(${test_target} PRIVATE Boost::openmethod Boost::unit_test_framework)
    add_test(NAME ${test_target} COMMAND ${test_target})
    add_dependencies(tests ${test_target})
endforeach()

function(boost_openmethod_add_step_by_step dir)
  set(add_test "")
  if(ARGC GREATER 1)
    set(add_test "${ARGV1}")
  else()
    set(add_test "ON")
  endif()

  file(GLOB subdirs "${dir}/*")

  foreach (subdir ${subdirs})
    string(REGEX REPLACE ".*/" "" subex ${subdir})
    file(GLOB cpp_files "${subdir}/*.cpp")
    set(target "boost_openmethod-${dir}_${subex}")
    add_executable(${target} ${cpp_files})
    target_link_libraries(${target} PRIVATE Boost::openmethod)
    set(output_dir openmethod/${dir}/${subex})
    set_target_properties(${target} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${output_dir}"
      LIBRARY_OUTPUT_DIRECTORY "${output_dir}"
      ARCHIVE_OUTPUT_DIRECTORY "${output_dir}"
    )
    if(${add_test})
      add_test(NAME "${target}" COMMAND "${target}")
    endif()
    add_dependencies(tests "${target}")
  endforeach()
endfunction()

boost_openmethod_add_step_by_step(rolex)
boost_openmethod_add_step_by_step(ambiguities OFF)
boost_openmethod_add_step_by_step(core_api)
boost_openmethod_add_step_by_step(custom_rtti)
boost_openmethod_add_step_by_step(virtual_ptr_alt)

if (NOT WIN32)
  add_subdirectory(shared_libs)
endif()

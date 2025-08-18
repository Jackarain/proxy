# Copyright  (c) 2016-2025 Antony Polukhin
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

if(NOT TARGET tests)
  add_custom_target(tests)
endif()

add_executable(dll_tests_cpp_mangling "cpp_mangling.cpp")
target_link_libraries(dll_tests_cpp_mangling Boost::dll)
add_test(NAME dll_tests_cpp_mangling COMMAND dll_tests_cpp_mangling)
add_dependencies(tests dll_tests_cpp_mangling)


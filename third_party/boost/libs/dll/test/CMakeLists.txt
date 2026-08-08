# Copyright Antony Polukhin, 2021-2026
# Copyright Fedor Osetrov, 2025-2026
#
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

if(NOT TARGET tests)
  add_custom_target(tests)
endif()

# Test libraries and plugins

add_library(dll_static_plugin STATIC ../example/tutorial4/static_plugin.cpp)
target_link_libraries(dll_static_plugin PRIVATE Boost::dll)

add_library(dll_static_refcounting_plugin STATIC ../example/tutorial8/refcounting_plugin.cpp)
target_link_libraries(dll_static_refcounting_plugin PRIVATE Boost::dll)

add_library(dll_test_library SHARED test_library.cpp)
target_link_libraries(dll_test_library PRIVATE Boost::dll Boost::fusion)

add_library(dll_empty_library SHARED empty_library.cpp)

add_library(dll_getting_started_library SHARED ../example/getting_started_library.cpp)
target_link_libraries(dll_getting_started_library PRIVATE Boost::dll)

add_library(dll_my_plugin_sum SHARED ../example/tutorial1/my_plugin_sum.cpp)
target_link_libraries(dll_my_plugin_sum PRIVATE Boost::dll)
set_target_properties(dll_my_plugin_sum
  PROPERTIES
    OUTPUT_NAME "my_plugin_sum"
)

add_library(dll_my_plugin_aggregator SHARED ../example/tutorial2/my_plugin_aggregator.cpp)
target_link_libraries(dll_my_plugin_aggregator PRIVATE Boost::dll)
set_target_properties(dll_my_plugin_aggregator
  PROPERTIES
    OUTPUT_NAME "my_plugin_aggregator"
)

add_library(dll_on_unload_lib SHARED ../example/tutorial6/on_unload_lib.cpp)
target_link_libraries(dll_on_unload_lib PRIVATE Boost::dll)

add_library(dll_library1 SHARED ../example/tutorial7/library1.cpp)
target_link_libraries(dll_library1 PRIVATE Boost::dll)

add_library(dll_library2 SHARED ../example/tutorial7/library2.cpp)
target_link_libraries(dll_library2 PRIVATE Boost::dll)

add_library(dll_refcounting_plugin SHARED ../example/tutorial8/refcounting_plugin.cpp)
target_link_libraries(dll_refcounting_plugin PRIVATE Boost::dll)
set_target_properties(dll_refcounting_plugin
  PROPERTIES
    OUTPUT_NAME "refcounting_plugin"
)

add_library(dll_cpp_plugin SHARED cpp_test_library.cpp)
target_link_libraries(dll_cpp_plugin PRIVATE Boost::variant)

add_library(dll_cpp_mangle_plugin SHARED cpp_ctti_type_name_parser_lib.cpp)
target_link_libraries(dll_cpp_mangle_plugin PRIVATE Boost::dll)

function(boost_dll_add_test name sources export_symbols)
  add_executable(${name} ${sources})
  set_target_properties(${name} PROPERTIES ENABLE_EXPORTS ${export_symbols})
  target_link_libraries(${name} PRIVATE Boost::dll Boost::filesystem)

  set(test_args)
  foreach(lib IN LISTS ARGN)
    list(APPEND test_args "$<TARGET_FILE:${lib}>")
  endforeach()
  add_test(NAME ${name} COMMAND ${name} ${test_args})

  if(ARGN)
    add_dependencies(${name} ${ARGN})
  endif()
  add_dependencies(tests ${name})
endfunction()

# Examples

boost_dll_add_test(dll_example_getting_started ../example/getting_started.cpp #[[export_symbols=]] FALSE dll_getting_started_library)

boost_dll_add_test(dll_example_tutorial1_std_shared_ptr ../example/tutorial1/tutorial1.cpp #[[export_symbols=]] FALSE dll_my_plugin_sum)
boost_dll_add_test(dll_example_tutorial1_boost_shared_ptr ../example/tutorial1/tutorial1.cpp #[[export_symbols=]] FALSE dll_my_plugin_sum)
target_compile_definitions(dll_example_tutorial1_boost_shared_ptr PRIVATE BOOST_DLL_USE_BOOST_SHARED_PTR)

boost_dll_add_test(dll_example_tutorial2 ../example/tutorial2/tutorial2.cpp #[[export_symbols=]] FALSE dll_my_plugin_aggregator)

boost_dll_add_test(dll_example_tutorial3 ../example/tutorial3/tutorial3.cpp #[[export_symbols=]] FALSE dll_my_plugin_sum dll_my_plugin_aggregator)

boost_dll_add_test(dll_example_tutorial4 ../example/tutorial4/load_self.cpp #[[export_symbols=]] TRUE)
target_link_libraries(dll_example_tutorial4 PRIVATE dll_static_plugin)

boost_dll_add_test(dll_example_tutorial5 ../example/tutorial5/load_all.cpp #[[export_symbols=]] TRUE dll_getting_started_library dll_my_plugin_aggregator dll_my_plugin_sum)
target_link_libraries(dll_example_tutorial5 PRIVATE dll_static_plugin)

boost_dll_add_test(dll_example_tutorial6 ../example/tutorial6/tutorial6.cpp #[[export_symbols=]] FALSE dll_on_unload_lib)

boost_dll_add_test(dll_example_tutorial7 ../example/tutorial7/tutorial7.cpp #[[export_symbols=]] FALSE dll_library1 dll_library2)

boost_dll_add_test(dll_example_tutorial8 ../example/tutorial8/tutorial8.cpp #[[export_symbols=]] FALSE dll_refcounting_plugin)
boost_dll_add_test(dll_example_tutorial8_static ../example/tutorial8/tutorial8_static.cpp #[[export_symbols=]] TRUE)
target_link_libraries(dll_example_tutorial8_static PRIVATE dll_static_refcounting_plugin)

boost_dll_add_test(dll_example_tutorial9 ../example/tutorial9/tutorial9.cpp #[[export_symbols=]] FALSE)

# Tests

add_executable(dll_test_link link1.cpp link2.cpp)
target_link_libraries(dll_test_link PRIVATE Boost::dll)

boost_dll_add_test(dll_test_structures structures_tests.cpp #[[export_symbols=]] FALSE)
boost_dll_add_test(dll_test_cpp_mangling cpp_mangling.cpp #[[export_symbols=]] FALSE)
boost_dll_add_test(dll_test_cpp_mangle cpp_mangle_test.cpp #[[export_symbols=]] FALSE dll_cpp_plugin)
target_link_libraries(dll_test_cpp_mangle PRIVATE Boost::variant)
boost_dll_add_test(dll_test_cpp_load cpp_load_test.cpp #[[export_symbols=]] FALSE dll_cpp_plugin)
target_link_libraries(dll_test_cpp_load PRIVATE Boost::variant)
boost_dll_add_test(dll_test_smart_library smart_library_test.cpp #[[export_symbols=]] FALSE dll_cpp_plugin)
boost_dll_add_test(dll_test_cpp_import cpp_import_test.cpp #[[export_symbols=]] FALSE dll_cpp_plugin)
target_link_libraries(dll_test_cpp_import PRIVATE Boost::variant)
if(LINUX)
  boost_dll_add_test(dll_test_cpp_template_method_linux template_method_linux_test.cpp #[[export_symbols=]] FALSE dll_cpp_plugin)
endif()
if(LINUX AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  boost_dll_add_test(dll_test_ctti_type_name_parser ctti_type_name_parser_test.cpp #[[export_symbols=]] FALSE dll_cpp_mangle_plugin)
  target_link_libraries(dll_test_ctti_type_name_parser PRIVATE Boost::variant)
endif()
boost_dll_add_test(dll_test_shared_library_load shared_library_load_test.cpp #[[export_symbols=]] FALSE dll_test_library dll_library1)
boost_dll_add_test(dll_test_shared_library_search_symbol shared_library_search_symbol_test.cpp #[[export_symbols=]] TRUE dll_test_library)
boost_dll_add_test(dll_test_shared_library_get_symbol shared_library_get_symbol_test.cpp #[[export_symbols=]] TRUE dll_test_library)
boost_dll_add_test(dll_test_symbol_runtime_info symbol_runtime_info_test.cpp #[[export_symbols=]] TRUE dll_test_library)
boost_dll_add_test(dll_test_shared_library_errors shared_library_errors.cpp #[[export_symbols=]] FALSE dll_test_library)
boost_dll_add_test(dll_test_library_info library_info_test.cpp #[[export_symbols=]] FALSE dll_test_library)
target_link_libraries(dll_test_library_info PRIVATE dll_static_plugin)
boost_dll_add_test(dll_test_broken_library_info broken_library_info_test.cpp #[[export_symbols=]] FALSE dll_test_library)
boost_dll_add_test(dll_test_empty_library_info empty_library_info_test.cpp #[[export_symbols=]] FALSE dll_empty_library)
boost_dll_add_test(dll_test_shared_library_concurrent_load shared_library_concurrent_load_test.cpp #[[export_symbols=]] FALSE
    dll_library1
    dll_library2
    dll_my_plugin_aggregator
    dll_refcounting_plugin
)

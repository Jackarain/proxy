# Copyright  (c) 2016-2026 Antony Polukhin
# Copyright  (c) 2025-2026 Fedor Osetrov
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

if(NOT TARGET tests)
  add_custom_target(tests)
endif()

if(MSVC)
    set(BOOST_TYPEINDEX_DETAIL_NO_RTTI "/GR-")
    set(BOOST_TYPEINDEX_DETAIL_RTTI "/GR")
else()
    set(BOOST_TYPEINDEX_DETAIL_NO_RTTI "-fno-rtti")
    set(BOOST_TYPEINDEX_DETAIL_RTTI "-frtti")
endif()

add_library(boost_type_index_test_lib_rtti SHARED test_lib.cpp)
target_compile_options(boost_type_index_test_lib_rtti PRIVATE ${BOOST_TYPEINDEX_DETAIL_RTTI})
target_link_libraries(boost_type_index_test_lib_rtti PRIVATE Boost::type_index)

add_library(boost_type_index_test_lib_anonymous_rtti SHARED test_lib_anonymous.cpp)
target_compile_options(boost_type_index_test_lib_anonymous_rtti PRIVATE ${BOOST_TYPEINDEX_DETAIL_RTTI})
target_link_libraries(boost_type_index_test_lib_anonymous_rtti PRIVATE Boost::type_index)

function(boost_type_index_test name sources)
    add_executable(${name} "${sources}")
    target_link_libraries(${name} PRIVATE Boost::core ${ARGN})
    add_test(NAME ${name} COMMAND ${name})
    add_dependencies(tests ${name})
endfunction()

function(boost_type_index_add_target target)
  if(BOOST_USE_MODULES)
    # C++ modules with different compile flags and definitions must be built separately
    add_library(${target})
    target_sources(${target}
      PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../"
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/../modules/boost_type_index.cppm"
    )
    target_compile_features(${target} PUBLIC cxx_std_20)
  else()
    add_library(${target} INTERFACE)
    target_link_libraries(${target} INTERFACE Boost::type_index)
  endif()

  target_include_directories(${target} ${__scope} ../include)
  target_link_libraries(${target}
    ${__scope}
        Boost::config
        Boost::container_hash
        Boost::throw_exception
  )
endfunction()

boost_type_index_test(type_index_test type_index_test.cpp Boost::type_index)
boost_type_index_test(type_index_constexpr_test type_index_constexpr_test.cpp Boost::type_index)
boost_type_index_test(type_index_ctti_print_name_test ctti_print_name.cpp Boost::type_index)
boost_type_index_test(type_index_runtime_cast_test type_index_runtime_cast_test.cpp Boost::type_index Boost::smart_ptr)

boost_type_index_test(type_index_crossmodule_test testing_crossmodule.cpp Boost::type_index boost_type_index_test_lib_rtti)
boost_type_index_test(type_index_crossmodule_anonymous_test testing_crossmodule_anonymous.cpp Boost::type_index boost_type_index_test_lib_anonymous_rtti)

boost_type_index_test(type_index_compare_ctti_stl_test compare_ctti_stl.cpp Boost::type_index)
boost_type_index_test(type_index_track_13621 track_13621.cpp Boost::type_index)

boost_type_index_test(type_index_ctti_alignment_test type_index_test_ctti_alignment.cpp Boost::type_index)

get_target_property(_use_import_std boost_type_index CXX_MODULE_STD)

file(GLOB EXAMPLE_FILES "../examples/*.cpp")
foreach (testsourcefile ${EXAMPLE_FILES})
    get_filename_component(testname ${testsourcefile} NAME_WE)

    if(testname STREQUAL "user_defined_typeinfo")
      continue()
    endif()

    boost_type_index_test(type_index_${testname}_example ${testsourcefile} Boost::type_index Boost::unordered)
    target_include_directories(type_index_${testname}_example PRIVATE ../examples)

    if (_use_import_std)
      continue()
    endif()

    if(testname STREQUAL "table_of_names")
      continue()
    endif()

    boost_type_index_test(type_index_${testname}_no_rtti_example ${testsourcefile} Boost::type_index_no_rtti Boost::unordered)
    target_include_directories(type_index_${testname}_no_rtti_example PRIVATE ../examples)
endforeach()

boost_type_index_add_target(boost_type_index_user_defined_typeinfo)
if(BOOST_USE_MODULES)
  target_compile_definitions(boost_type_index_user_defined_typeinfo PRIVATE "BOOST_TYPE_INDEX_USER_TYPEINDEX=<../examples/user_defined_typeinfo.hpp>")
endif()

boost_type_index_test(type_index_user_defined_typeinfo_example ../examples/user_defined_typeinfo.cpp boost_type_index_user_defined_typeinfo)
target_include_directories(type_index_user_defined_typeinfo_example PRIVATE ../examples)

# CMake currently does not support standard stl module with different compile flags
if (NOT _use_import_std)

boost_type_index_add_target(boost_type_index_user_defined_typeinfo_no_rtti)
if(BOOST_USE_MODULES)
  target_compile_definitions(boost_type_index_user_defined_typeinfo_no_rtti PRIVATE "BOOST_TYPE_INDEX_USER_TYPEINDEX=<../examples/user_defined_typeinfo.hpp>")
endif()
target_compile_options(boost_type_index_user_defined_typeinfo_no_rtti ${__scope} ${BOOST_TYPEINDEX_DETAIL_NO_RTTI})

boost_type_index_test(type_index_user_defined_typeinfo_no_rtti_example ../examples/user_defined_typeinfo.cpp boost_type_index_user_defined_typeinfo_no_rtti)
target_include_directories(type_index_user_defined_typeinfo_no_rtti_example PRIVATE ../examples)

foreach(target IN ITEMS
    boost_type_index_no_rtti
    boost_type_index_rtti_no_compat
    boost_type_index_no_rtti_no_compat
)
  boost_type_index_add_target(${target})
endforeach()

target_compile_options(boost_type_index_no_rtti ${__scope} ${BOOST_TYPEINDEX_DETAIL_NO_RTTI})
target_compile_options(boost_type_index_no_rtti_no_compat ${__scope} ${BOOST_TYPEINDEX_DETAIL_NO_RTTI})
target_compile_definitions(boost_type_index_rtti_no_compat ${__scope} BOOST_TYPE_INDEX_FORCE_NO_RTTI_COMPATIBILITY=1)
target_compile_definitions(boost_type_index_no_rtti_no_compat ${__scope} BOOST_TYPE_INDEX_FORCE_NO_RTTI_COMPATIBILITY=1)

add_library(Boost::type_index_no_rtti ALIAS boost_type_index_no_rtti)
add_library(Boost::type_index_rtti_no_compat ALIAS boost_type_index_rtti_no_compat)
add_library(Boost::type_index_no_rtti_no_compat ALIAS boost_type_index_no_rtti_no_compat)

# Making libraries that CANNOT work between rtti-on/rtti-off modules
add_library(boost_type_index_test_lib_nortti SHARED test_lib.cpp)
target_compile_options(boost_type_index_test_lib_nortti PRIVATE ${BOOST_TYPEINDEX_DETAIL_NO_RTTI})
target_link_libraries(boost_type_index_test_lib_nortti PRIVATE Boost::type_index_no_rtti)

add_library(boost_type_index_test_lib_anonymous_nortti SHARED test_lib_anonymous.cpp)
target_compile_options(boost_type_index_test_lib_anonymous_nortti PRIVATE ${BOOST_TYPEINDEX_DETAIL_NO_RTTI})
target_link_libraries(boost_type_index_test_lib_anonymous_nortti PRIVATE Boost::type_index_no_rtti)

# Making libraries that can work between rtti-on/rtti-off modules
add_library(boost_type_index_test_lib_nortti_compat SHARED test_lib.cpp)
target_compile_options(boost_type_index_test_lib_nortti_compat PRIVATE ${BOOST_TYPEINDEX_DETAIL_NO_RTTI})
target_compile_definitions(boost_type_index_test_lib_nortti_compat PUBLIC BOOST_TYPE_INDEX_FORCE_NO_RTTI_COMPATIBILITY=1)
target_link_libraries(boost_type_index_test_lib_nortti_compat PUBLIC Boost::type_index_no_rtti_no_compat)

add_library(boost_type_index_test_lib_rtti_compat SHARED test_lib.cpp)
target_compile_options(boost_type_index_test_lib_rtti_compat PRIVATE ${BOOST_TYPEINDEX_DETAIL_RTTI})
target_compile_definitions(boost_type_index_test_lib_rtti_compat PUBLIC BOOST_TYPE_INDEX_FORCE_NO_RTTI_COMPATIBILITY=1)
target_link_libraries(boost_type_index_test_lib_rtti_compat PUBLIC Boost::type_index_rtti_no_compat)

boost_type_index_test(type_index_test_no_rtti type_index_test.cpp Boost::type_index_no_rtti)

boost_type_index_test(type_index_crossmodule_no_rtti_test testing_crossmodule.cpp Boost::type_index_no_rtti boost_type_index_test_lib_nortti)

# # Mixing RTTI on and off
if(NOT MSVC)  # MSVC sometimes overrides the /GR- and the tests link
    boost_type_index_test(type_index_crossmodule_no_rtti_rtti_compat_test testing_crossmodule.cpp boost_type_index_test_lib_rtti_compat)
    boost_type_index_test(type_index_crossmodule_rtti_nortti_compat_test testing_crossmodule.cpp boost_type_index_test_lib_nortti_compat)
endif()

endif()

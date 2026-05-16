# Copyright  (c) 2016-2026 Antony Polukhin
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

include(BoostTest OPTIONAL RESULT_VARIABLE HAVE_BOOST_TEST)

if(NOT HAVE_BOOST_TEST)
  return()
endif()

if(MSVC)
    set(BOOST_TYPEINDEX_DETAIL_NO_RTTI "/GR-")
    set(BOOST_TYPEINDEX_DETAIL_RTTI "/GR")
else()
    set(BOOST_TYPEINDEX_DETAIL_NO_RTTI "-fno-rtti")
    set(BOOST_TYPEINDEX_DETAIL_RTTI "-frtti")
endif()

# Making libraries that CANNOT work between rtti-on/rtti-off modules
add_library(boost_type_index_test_lib_nortti SHARED test_lib.cpp)
target_compile_options(boost_type_index_test_lib_nortti PRIVATE ${BOOST_TYPEINDEX_DETAIL_NO_RTTI})
target_link_libraries(boost_type_index_test_lib_nortti PRIVATE Boost::type_index)

add_library(boost_type_index_test_lib_anonymous_nortti SHARED test_lib_anonymous.cpp)
target_compile_options(boost_type_index_test_lib_anonymous_nortti PRIVATE ${BOOST_TYPEINDEX_DETAIL_NO_RTTI})
target_link_libraries(boost_type_index_test_lib_anonymous_nortti PRIVATE Boost::type_index)

add_library(boost_type_index_test_lib_rtti SHARED test_lib.cpp)
target_compile_options(boost_type_index_test_lib_rtti PRIVATE ${BOOST_TYPEINDEX_DETAIL_RTTI})
target_link_libraries(boost_type_index_test_lib_rtti PRIVATE Boost::type_index)

add_library(boost_type_index_test_lib_anonymous_rtti SHARED test_lib_anonymous.cpp)
target_compile_options(boost_type_index_test_lib_anonymous_rtti PRIVATE ${BOOST_TYPEINDEX_DETAIL_RTTI})
target_link_libraries(boost_type_index_test_lib_anonymous_rtti PRIVATE Boost::type_index)

# Making libraries that can work between rtti-on/rtti-off modules
add_library(boost_type_index_test_lib_nortti_compat SHARED test_lib.cpp)
target_compile_options(boost_type_index_test_lib_nortti_compat PRIVATE ${BOOST_TYPEINDEX_DETAIL_NO_RTTI})
target_compile_definitions(boost_type_index_test_lib_nortti_compat PUBLIC BOOST_TYPE_INDEX_FORCE_NO_RTTI_COMPATIBILITY=1)
target_link_libraries(boost_type_index_test_lib_nortti_compat PRIVATE Boost::type_index)

add_library(boost_type_index_test_lib_rtti_compat SHARED test_lib.cpp)
target_compile_options(boost_type_index_test_lib_rtti_compat PRIVATE ${BOOST_TYPEINDEX_DETAIL_RTTI})
target_compile_definitions(boost_type_index_test_lib_rtti_compat PUBLIC BOOST_TYPE_INDEX_FORCE_NO_RTTI_COMPATIBILITY=1)
target_link_libraries(boost_type_index_test_lib_rtti_compat PRIVATE Boost::type_index)

set(BOOST_TEST_LINK_LIBRARIES Boost::type_index Boost::core)

boost_test(TYPE run SOURCES type_index_test.cpp)
boost_test(TYPE run SOURCES type_index_runtime_cast_test.cpp LINK_LIBRARIES Boost::smart_ptr)
boost_test(TYPE run SOURCES type_index_constexpr_test.cpp)
boost_test(TYPE run SOURCES type_index_test.cpp COMPILE_OPTIONS ${BOOST_TYPEINDEX_DETAIL_NO_RTTI} NAME type_index_test_no_rtti)
boost_test(TYPE run SOURCES ctti_print_name.cpp)

boost_test(TYPE run SOURCES testing_crossmodule.cpp
    LINK_LIBRARIES boost_type_index_test_lib_rtti
)
boost_test(TYPE run SOURCES testing_crossmodule.cpp
    LINK_LIBRARIES boost_type_index_test_lib_nortti
    COMPILE_OPTIONS ${BOOST_TYPEINDEX_DETAIL_NO_RTTI}
    NAME testing_crossmodule_no_rtti
)
boost_test(TYPE run SOURCES testing_crossmodule_anonymous.cpp
    LINK_LIBRARIES boost_type_index_test_lib_anonymous_rtti
)

boost_test(TYPE run SOURCES compare_ctti_stl.cpp)
boost_test(TYPE run SOURCES track_13621.cpp)

boost_test(TYPE compile-fail SOURCES type_index_test_ctti_copy_fail.cpp)
boost_test(TYPE compile-fail SOURCES type_index_test_ctti_construct_fail.cpp)
boost_test(TYPE compile SOURCES type_index_test_ctti_alignment.cpp)

# Mixing RTTI on and off
if(NOT MSVC)  # MSVC sometimes overrides the /GR- and the tests link
    boost_test(TYPE link-fail SOURCES testing_crossmodule.cpp
            LINK_LIBRARIES boost_type_index_test_lib_rtti
            COMPILE_OPTIONS  ${BOOST_TYPEINDEX_DETAIL_NO_RTTI}
            NAME link_fail_nortti_rtti)
    boost_test(TYPE link-fail SOURCES testing_crossmodule.cpp
            LINK_LIBRARIES boost_type_index_test_lib_nortti
            NAME link_fail_rtti_nortti)

    boost_test(TYPE run SOURCES testing_crossmodule.cpp
        LINK_LIBRARIES boost_type_index_test_lib_rtti_compat
        COMPILE_OPTIONS ${BOOST_TYPEINDEX_DETAIL_NO_RTTI}
        COMPILE_DEFINITIONS BOOST_TYPE_INDEX_FORCE_NO_RTTI_COMPATIBILITY=1
        NAME testing_crossmodule_nortti_rtti_compat
    )
    boost_test(TYPE run SOURCES testing_crossmodule.cpp
        LINK_LIBRARIES boost_type_index_test_lib_nortti_compat
        COMPILE_DEFINITIONS BOOST_TYPE_INDEX_FORCE_NO_RTTI_COMPATIBILITY=1
        NAME testing_crossmodule_rtti_nortti_compat
    )
endif()

file(GLOB EXAMPLE_FILES "../examples/*.cpp")
foreach (testsourcefile ${EXAMPLE_FILES})
    boost_test(TYPE run SOURCES ${testsourcefile} INCLUDE_DIRECTORIES ../examples LINK_LIBRARIES Boost::unordered)

    get_filename_component(testname ${testsourcefile} NAME_WE)
    if(NOT testname STREQUAL "table_of_names")
        boost_test(TYPE run SOURCES ${testsourcefile} NAME ${testname}_no_rtti COMPILE_OPTIONS  ${BOOST_TYPEINDEX_DETAIL_NO_RTTI} INCLUDE_DIRECTORIES ../examples LINK_LIBRARIES Boost::unordered)
    endif()
endforeach()

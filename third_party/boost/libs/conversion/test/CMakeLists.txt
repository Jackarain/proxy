# Copyright (c) 2016-2026 Antony Polukhin
# Copyright (c) 2025-2026 Fedor Osetrov
#
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

if(NOT TARGET tests)
  add_custom_target(tests)
endif()

set(TEST_SOURCES
  cast_test.cpp
  implicit_cast.cpp
  polymorphic_cast_test.cpp
)
foreach (testsourcefile ${TEST_SOURCES})
    get_filename_component(testname ${testsourcefile} NAME_WE)
    add_executable(conversion_${testname} ${testsourcefile})
    target_link_libraries(conversion_${testname} Boost::conversion Boost::core Boost::smart_ptr)
    add_test(NAME conversion_${testname} COMMAND conversion_${testname})
    add_dependencies(tests conversion_${testname})
endforeach()

#
# Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
# Copyright (c) 2021 DMitry Arkhipov (grisumbras@gmail.com)
# Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/url
#

# Files
set(TEST_FILES Jamfile test_test_suite.cpp)

# Test target
add_executable(boost_url_extra EXCLUDE_FROM_ALL ${TEST_FILES})
target_link_libraries(boost_url_extra PRIVATE Boost::url boost_url_test_suite_with_main)

# Folders
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${TEST_FILES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/../../extra/test_suite PREFIX "_extra" FILES ${SUITE_FILES})

# CTest Target
add_test(NAME boost_url_extra COMMAND boost_url_extra)
add_dependencies(tests boost_url_extra)


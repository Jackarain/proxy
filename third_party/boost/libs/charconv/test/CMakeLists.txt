# Copyright 2018, 2019 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

include(BoostTestJamfile OPTIONAL RESULT_VARIABLE HAVE_BOOST_TEST)

if(HAVE_BOOST_TEST)

    enable_testing()

    if(BOOST_CHARCONV_ENABLE_CUDA)

        message(STATUS "Building Boost.charconv with CUDA")

        find_package(CUDA REQUIRED)
        enable_language(CUDA)
        set(CMAKE_CUDA_EXTENSIONS OFF)

        boost_test_jamfile(FILE cuda_jamfile LINK_LIBRARIES Boost::charconv Boost::core Boost::assert ${CUDA_LIBRARIES} COMPILE_DEFINITIONS BOOST_CHARCONV_ENABLE_CUDA=1 INCLUDE_DIRECTORIES ${CUDA_INCLUDE_DIRS} )

    else()

        # https://crascit.com/2015/03/28/enabling-cxx11-in-cmake/
        set(CMAKE_CXX_EXTENSIONS OFF)
        boost_test_jamfile(FILE Jamfile LINK_LIBRARIES Boost::charconv Boost::core Boost::assert)

    endif()

endif()

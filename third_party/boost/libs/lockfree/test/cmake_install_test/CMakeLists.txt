cmake_minimum_required(VERSION 3.5...3.16)

project(cmake_install_test LANGUAGES CXX)

if(BOOST_CI_INSTALL_TEST)
    find_package(Boost CONFIG QUIET COMPONENTS lockfree)
    if(NOT TARGET Boost::lockfree)
        add_library(Boost::lockfree INTERFACE IMPORTED)
        set_target_properties(Boost::lockfree PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}")
    endif()
else()
    find_package(boost_lockfree REQUIRED)
endif()

add_executable(main main.cpp)
target_link_libraries(main Boost::lockfree)

enable_testing()
add_test(main main)

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG>)

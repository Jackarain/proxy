# Copyright 2024 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

link_libraries(Boost::hash2 Boost::core Boost::unordered)

add_executable(buffer buffer.cpp)
add_executable(unordered unordered.cpp)
add_executable(average average.cpp)
add_executable(keys keys.cpp)

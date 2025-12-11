# Copyright (c) 2018-2024 Jean-Louis Leroy
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt
# or copy at http://www.boost.org/LICENSE_1_0.txt)

add_executable(headers_namespaces main.cpp cat.cpp dog.cpp)
target_link_libraries(headers_namespaces Boost::openmethod)
add_test(NAME headers_namespaces COMMAND headers_namespaces)

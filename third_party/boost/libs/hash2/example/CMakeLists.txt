# Copyright 2024 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

link_libraries(Boost::hash2 Boost::unordered Boost::endian)

add_executable(md5sum md5sum.cpp)
add_executable(hash2sum hash2sum.cpp)
add_executable(compile_time compile_time.cpp)
add_executable(compile_time_2 compile_time_2.cpp)
add_executable(hash_without_seed hash_without_seed.cpp)
add_executable(hash_with_uint64_seed hash_with_uint64_seed.cpp)
add_executable(hash_with_byte_seed hash_with_byte_seed.cpp)
add_executable(hash_with_any_seed hash_with_any_seed.cpp)
add_executable(hash_32_64 hash_32_64.cpp)
add_executable(xxh128_from_xxh64 xxh128_from_xxh64.cpp)
add_executable(json_value json_value.cpp)
target_link_libraries(json_value PRIVATE Boost::json)

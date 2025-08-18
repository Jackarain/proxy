set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # warning level 4
  add_compile_options(/W4)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_executable(float_sort_test float_sort_test.cpp)
target_include_directories(float_sort_test PRIVATE ../include)
add_test(NAME float_sort_test COMMAND $<TARGET_FILE:float_sort_test>)

add_executable(integer_sort_test integer_sort_test.cpp)
target_include_directories(integer_sort_test PRIVATE ../include)
add_test(NAME integer_sort_test COMMAND $<TARGET_FILE:integer_sort_test>)

add_executable(sort_detail_test sort_detail_test.cpp)
target_include_directories(sort_detail_test PRIVATE ../include)
add_test(NAME sort_detail_test COMMAND $<TARGET_FILE:sort_detail_test>)

add_executable(string_sort_test string_sort_test.cpp)
target_include_directories(string_sort_test PRIVATE ../include)
add_test(NAME string_sort_test COMMAND $<TARGET_FILE:string_sort_test>)

add_executable(test_block_indirect_sort test_block_indirect_sort.cpp)
target_include_directories(test_block_indirect_sort PRIVATE ../include)
add_test(NAME test_block_indirect_sort COMMAND $<TARGET_FILE:test_block_indirect_sort>)

add_executable(test_flat_stable_sort test_flat_stable_sort.cpp)
target_include_directories(test_flat_stable_sort PRIVATE ../include)
add_test(NAME test_flat_stable_sort COMMAND $<TARGET_FILE:test_flat_stable_sort>)

add_executable(test_insert_sort test_insert_sort.cpp)
target_include_directories(test_insert_sort PRIVATE ../include)
add_test(NAME test_insert_sort COMMAND $<TARGET_FILE:test_insert_sort>)

add_executable(test_parallel_stable_sort test_parallel_stable_sort.cpp)
target_include_directories(test_parallel_stable_sort PRIVATE ../include)
add_test(NAME test_parallel_stable_sort COMMAND $<TARGET_FILE:test_parallel_stable_sort>)

add_executable(test_pdqsort test_pdqsort.cpp)
target_include_directories(test_pdqsort PRIVATE ../include)
add_test(NAME test_pdqsort COMMAND $<TARGET_FILE:test_pdqsort>)

add_executable(test_sample_sort test_sample_sort.cpp)
target_include_directories(test_sample_sort PRIVATE ../include)
add_test(NAME test_sample_sort COMMAND $<TARGET_FILE:test_sample_sort>)

add_executable(test_spinsort test_spinsort.cpp)
target_include_directories(test_spinsort PRIVATE ../include)
add_test(NAME test_spinsort COMMAND $<TARGET_FILE:test_spinsort>)

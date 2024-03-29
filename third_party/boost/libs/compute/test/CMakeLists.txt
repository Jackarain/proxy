# ---------------------------------------------------------------------------
#  Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
#
#  Distributed under the Boost Software License, Version 1.0
#  See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt
#
# ---------------------------------------------------------------------------

include_directories(../include)

set(BOOST_COMPONENTS unit_test_framework)

if(${BOOST_COMPUTE_USE_CPP11})
  # allow tests to use C++11 features
  add_definitions(-DBOOST_COMPUTE_USE_CPP11)
endif()

if (${BOOST_COMPUTE_USE_OFFLINE_CACHE})
  set(BOOST_COMPONENTS ${BOOST_COMPONENTS} system filesystem)
  add_definitions(-DBOOST_COMPUTE_USE_OFFLINE_CACHE)
endif()

if(${BOOST_COMPUTE_THREAD_SAFE} AND NOT ${BOOST_COMPUTE_USE_CPP11})
  set(BOOST_COMPONENTS ${BOOST_COMPONENTS} system thread)
endif()

if(MSVC AND BOOST_COMPONENTS)
  set(BOOST_COMPONENTS ${BOOST_COMPONENTS} chrono)
endif()

if(BOOST_COMPONENTS)
  list(REMOVE_DUPLICATES BOOST_COMPONENTS)
endif()
find_package(Boost 1.54 REQUIRED COMPONENTS ${BOOST_COMPONENTS})

if(NOT MSVC)
  add_definitions(-DBOOST_TEST_DYN_LINK)
else()
  if(MSVC AND ${BOOST_COMPUTE_BOOST_ALL_DYN_LINK})
      add_definitions(-DBOOST_TEST_DYN_LINK)
  endif()
endif()

# enable automatic kernel compilation error messages for tests
add_definitions(-DBOOST_COMPUTE_DEBUG_KERNEL_COMPILATION)

# enable code coverage generation (only with GCC for now)
if(${BOOST_COMPUTE_ENABLE_COVERAGE} AND ${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
  add_definitions(-fprofile-arcs -ftest-coverage)
endif()

# add path to test data dir
add_definitions(-DBOOST_COMPUTE_TEST_DATA_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data")

function(add_compute_test TEST_NAME TEST_SOURCE)
  get_filename_component(TEST_TARGET ${TEST_SOURCE} NAME_WE)
  add_executable(${TEST_TARGET} ${TEST_SOURCE})
  target_link_libraries(${TEST_TARGET}
    ${OpenCL_LIBRARIES}
    ${Boost_LIBRARIES}
  )

  # link with coverage library
  if(${BOOST_COMPUTE_ENABLE_COVERAGE} AND ${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    target_link_libraries(${TEST_TARGET} -fprofile-arcs -ftest-coverage)
  endif()

  add_test(${TEST_NAME} ${TEST_TARGET})
endfunction()

add_compute_test("core.buffer" test_buffer.cpp)
add_compute_test("core.closure" test_closure.cpp)
add_compute_test("core.command_queue" test_command_queue.cpp)
add_compute_test("core.context" test_context.cpp)
add_compute_test("core.device" test_device.cpp)
add_compute_test("core.event" test_event.cpp)
add_compute_test("core.function" test_function.cpp)
add_compute_test("core.kernel" test_kernel.cpp)
add_compute_test("core.pipe" test_pipe.cpp)
add_compute_test("core.platform" test_platform.cpp)
add_compute_test("core.program" test_program.cpp)
add_compute_test("core.system" test_system.cpp)
add_compute_test("core.type_traits" test_type_traits.cpp)
add_compute_test("core.user_event" test_user_event.cpp)

add_compute_test("utility.extents" test_extents.cpp)
add_compute_test("utility.invoke" test_invoke.cpp)
add_compute_test("utility.program_cache" test_program_cache.cpp)
add_compute_test("utility.wait_list" test_wait_list.cpp)

add_compute_test("algorithm.accumulate" test_accumulate.cpp)
add_compute_test("algorithm.adjacent_difference" test_adjacent_difference.cpp)
add_compute_test("algorithm.adjacent_find" test_adjacent_find.cpp)
add_compute_test("algorithm.any_all_none_of" test_any_all_none_of.cpp)
add_compute_test("algorithm.binary_search" test_binary_search.cpp)
add_compute_test("algorithm.copy" test_copy.cpp)
add_compute_test("algorithm.copy_type_mismatch" test_copy_type_mismatch.cpp)
add_compute_test("algorithm.copy_if" test_copy_if.cpp)
add_compute_test("algorithm.count" test_count.cpp)
add_compute_test("algorithm.equal" test_equal.cpp)
add_compute_test("algorithm.equal_range" test_equal_range.cpp)
add_compute_test("algorithm.extrema" test_extrema.cpp)
add_compute_test("algorithm.fill" test_fill.cpp)
add_compute_test("algorithm.find" test_find.cpp)
add_compute_test("algorithm.find_end" test_find_end.cpp)
add_compute_test("algorithm.for_each" test_for_each.cpp)
add_compute_test("algorithm.gather" test_gather.cpp)
add_compute_test("algorithm.generate" test_generate.cpp)
add_compute_test("algorithm.includes" test_includes.cpp)
add_compute_test("algorithm.inner_product" test_inner_product.cpp)
add_compute_test("algorithm.inplace_merge" test_inplace_merge.cpp)
add_compute_test("algorithm.inplace_reduce" test_inplace_reduce.cpp)
add_compute_test("algorithm.insertion_sort" test_insertion_sort.cpp)
add_compute_test("algorithm.iota" test_iota.cpp)
add_compute_test("algorithm.is_permutation" test_is_permutation.cpp)
add_compute_test("algorithm.is_sorted" test_is_sorted.cpp)
add_compute_test("algorithm.merge_sort_gpu" test_merge_sort_gpu.cpp)
add_compute_test("algorithm.merge" test_merge.cpp)
add_compute_test("algorithm.mismatch" test_mismatch.cpp)
add_compute_test("algorithm.next_permutation" test_next_permutation.cpp)
add_compute_test("algorithm.nth_element" test_nth_element.cpp)
add_compute_test("algorithm.partial_sum" test_partial_sum.cpp)
add_compute_test("algorithm.partition" test_partition.cpp)
add_compute_test("algorithm.partition_point" test_partition_point.cpp)
add_compute_test("algorithm.prev_permutation" test_prev_permutation.cpp)
add_compute_test("algorithm.radix_sort" test_radix_sort.cpp)
add_compute_test("algorithm.radix_sort_by_key" test_radix_sort_by_key.cpp)
add_compute_test("algorithm.random_fill" test_random_fill.cpp)
add_compute_test("algorithm.random_shuffle" test_random_shuffle.cpp)
add_compute_test("algorithm.reduce" test_reduce.cpp)
add_compute_test("algorithm.reduce_by_key" test_reduce_by_key.cpp)
add_compute_test("algorithm.remove" test_remove.cpp)
add_compute_test("algorithm.replace" test_replace.cpp)
add_compute_test("algorithm.reverse" test_reverse.cpp)
add_compute_test("algorithm.rotate" test_rotate.cpp)
add_compute_test("algorithm.rotate_copy" test_rotate_copy.cpp)
add_compute_test("algorithm.scan" test_scan.cpp)
add_compute_test("algorithm.scatter" test_scatter.cpp)
add_compute_test("algorithm.scatter_if" test_scatter_if.cpp)
add_compute_test("algorithm.search" test_search.cpp)
add_compute_test("algorithm.search_n" test_search_n.cpp)
add_compute_test("algorithm.set_difference" test_set_difference.cpp)
add_compute_test("algorithm.set_intersection" test_set_intersection.cpp)
add_compute_test("algorithm.set_symmetric_difference" test_set_symmetric_difference.cpp)
add_compute_test("algorithm.set_union" test_set_union.cpp)
add_compute_test("algorithm.sort" test_sort.cpp)
add_compute_test("algorithm.sort_by_key" test_sort_by_key.cpp)
add_compute_test("algorithm.stable_partition" test_stable_partition.cpp)
add_compute_test("algorithm.stable_sort" test_stable_sort.cpp)
add_compute_test("algorithm.stable_sort_by_key" test_stable_sort_by_key.cpp)
add_compute_test("algorithm.transform" test_transform.cpp)
add_compute_test("algorithm.transform_if" test_transform_if.cpp)
add_compute_test("algorithm.transform_reduce" test_transform_reduce.cpp)
add_compute_test("algorithm.unique" test_unique.cpp)
add_compute_test("algorithm.unique_copy" test_unique_copy.cpp)
add_compute_test("algorithm.lexicographical_compare" test_lexicographical_compare.cpp)

add_compute_test("allocator.buffer_allocator" test_buffer_allocator.cpp)
add_compute_test("allocator.pinned_allocator" test_pinned_allocator.cpp)

add_compute_test("async.wait" test_async_wait.cpp)
add_compute_test("async.wait_guard" test_async_wait_guard.cpp)

add_compute_test("container.array" test_array.cpp)
add_compute_test("container.dynamic_bitset" test_dynamic_bitset.cpp)
add_compute_test("container.flat_map" test_flat_map.cpp)
add_compute_test("container.flat_set" test_flat_set.cpp)
add_compute_test("container.mapped_view" test_mapped_view.cpp)
add_compute_test("container.stack" test_stack.cpp)
add_compute_test("container.string" test_string.cpp)
add_compute_test("container.valarray" test_valarray.cpp)
add_compute_test("container.vector" test_vector.cpp)

add_compute_test("exception.context_error" test_context_error.cpp)
add_compute_test("exception.no_device_found" test_no_device_found.cpp)
add_compute_test("exception.opencl_error" test_opencl_error.cpp)
add_compute_test("exception.unsupported_extension" test_unsupported_extension.cpp)

add_compute_test("functional.as" test_functional_as.cpp)
add_compute_test("functional.bind" test_functional_bind.cpp)
add_compute_test("functional.convert" test_functional_convert.cpp)
add_compute_test("functional.get" test_functional_get.cpp)
add_compute_test("functional.hash" test_functional_hash.cpp)
add_compute_test("functional.identity" test_functional_identity.cpp)
add_compute_test("functional.popcount" test_functional_popcount.cpp)
add_compute_test("functional.unpack" test_functional_unpack.cpp)

add_compute_test("image.image1d" test_image1d.cpp)
add_compute_test("image.image2d" test_image2d.cpp)
add_compute_test("image.image3d" test_image3d.cpp)
add_compute_test("image.image_sampler" test_image_sampler.cpp)

add_compute_test("iterator.buffer_iterator" test_buffer_iterator.cpp)
add_compute_test("iterator.constant_iterator" test_constant_iterator.cpp)
add_compute_test("iterator.counting_iterator" test_counting_iterator.cpp)
add_compute_test("iterator.discard_iterator" test_discard_iterator.cpp)
add_compute_test("iterator.function_input_iterator" test_function_input_iterator.cpp)
add_compute_test("iterator.permutation_iterator" test_permutation_iterator.cpp)
add_compute_test("iterator.strided_iterator" test_strided_iterator.cpp)
add_compute_test("iterator.transform_iterator" test_transform_iterator.cpp)
add_compute_test("iterator.zip_iterator" test_zip_iterator.cpp)

add_compute_test("memory.local_buffer" test_local_buffer.cpp)
add_compute_test("memory.svm_ptr" test_svm_ptr.cpp)

add_compute_test("random.bernoulli_distribution" test_bernoulli_distribution.cpp)
add_compute_test("random.discrete_distribution" test_discrete_distribution.cpp)
add_compute_test("random.linear_congruential_engine" test_linear_congruential_engine.cpp)
add_compute_test("random.mersenne_twister_engine" test_mersenne_twister_engine.cpp)
add_compute_test("random.threefry_engine" test_threefry_engine.cpp)
add_compute_test("random.normal_distribution" test_normal_distribution.cpp)
add_compute_test("random.uniform_int_distribution" test_uniform_int_distribution.cpp)
add_compute_test("random.uniform_real_distribution" test_uniform_real_distribution.cpp)

add_compute_test("types.fundamental" test_types.cpp)
add_compute_test("types.complex" test_complex.cpp)
add_compute_test("types.pair" test_pair.cpp)
add_compute_test("types.tuple" test_tuple.cpp)
add_compute_test("types.struct" test_struct.cpp)

add_compute_test("type_traits.result_of" test_result_of.cpp)

add_compute_test("experimental.clamp_range" test_clamp_range.cpp)
add_compute_test("experimental.malloc" test_malloc.cpp)
add_compute_test("experimental.sort_by_transform" test_sort_by_transform.cpp)
add_compute_test("experimental.tabulate" test_tabulate.cpp)

# miscellaneous tests
add_compute_test("misc.amd_cpp_kernel_language" test_amd_cpp_kernel_language.cpp)
add_compute_test("misc.lambda" test_lambda.cpp)
add_compute_test("misc.user_defined_types" test_user_defined_types.cpp)
add_compute_test("misc.literal_conversion" test_literal_conversion.cpp)

# extra tests (interop tests, linkage tests, etc.)
add_subdirectory(extra)

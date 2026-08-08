//  Copyright Matt Borland 2024 - 2026.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <iomanip>
#include <vector>
#include <random>
#include <limits>
#include <boost/charconv/detail/integer_search_trees.hpp>
#include "cuda_managed_ptr.hpp"
#include "stopwatch.hpp"

// For the CUDA runtime routines (prefixed with "cuda_")
#include <cuda_runtime.h>

using test_type = std::uint32_t;

__global__ void cuda_test(const test_type *in, int *out, int numElements)
{
    int i = blockDim.x * blockIdx.x + threadIdx.x;

    if (i < numElements)
    {
        out[i] = boost::charconv::detail::num_digits(in[i]);
    }
}

/**
 * Host main routine
 */
int main(void)
{
    std::mt19937_64 rng {42};

    // Error code to check return values for CUDA calls
    cudaError_t err = cudaSuccess;

    // Print the vector length to be used, and compute its size
    int numElements = 50000;
    std::cout << "[Vector operation on " << numElements << " elements]" << std::endl;

    // Allocate the managed input vector
    cuda_managed_ptr<test_type> input_vector(numElements);

    // Allocate the managed output vector
    cuda_managed_ptr<int> output_vector(numElements);

    // Initialize the input vectors with random values across the full range
    std::uniform_int_distribution<test_type> dist {1, (std::numeric_limits<test_type>::max)()};
    for (std::size_t i = 0; i < numElements; ++i)
    {
        input_vector[i] = dist(rng);
    }

    // Also test boundary values at specific digit counts
    // 1-digit: 1-9, 2-digit: 10-99, ..., 10-digit: 1000000000-4294967295
    test_type boundaries[] = {
        UINT32_C(0),
        UINT32_C(1),
        UINT32_C(9),
        UINT32_C(10),
        UINT32_C(99),
        UINT32_C(100),
        UINT32_C(999),
        UINT32_C(1000),
        UINT32_C(9999),
        UINT32_C(10000),
        UINT32_C(99999),
        UINT32_C(100000),
        UINT32_C(999999),
        UINT32_C(1000000),
        UINT32_C(9999999),
        UINT32_C(10000000),
        UINT32_C(99999999),
        UINT32_C(100000000),
        UINT32_C(999999999),
        UINT32_C(1000000000),
        UINT32_C(4294967295)
    };
    int num_boundaries = sizeof(boundaries) / sizeof(boundaries[0]);
    for (int i = 0; i < num_boundaries && i < numElements; ++i)
    {
        input_vector[i] = boundaries[i];
    }

    // Launch the CUDA Kernel
    int threadsPerBlock = 256;
    int blocksPerGrid = (numElements + threadsPerBlock - 1) / threadsPerBlock;
    std::cout << "CUDA kernel launch with " << blocksPerGrid << " blocks of " << threadsPerBlock << " threads" << std::endl;

    watch w;

    cuda_test<<<blocksPerGrid, threadsPerBlock>>>(input_vector.get(), output_vector.get(), numElements);
    cudaDeviceSynchronize();

    std::cout << "CUDA kernal done in: " << w.elapsed() << "s" << std::endl;

    err = cudaGetLastError();

    if (err != cudaSuccess)
    {
        std::cerr << "Failed to launch kernel (error code " << cudaGetErrorString(err) << ")!" << std::endl;
        return EXIT_FAILURE;
    }

    // Verify that the result vector is correct
    w.reset();
    for(int i = 0; i < numElements; ++i)
    {
        int cpu_result = boost::charconv::detail::num_digits(input_vector[i]);
        if (output_vector[i] != cpu_result)
        {
            std::cerr << "Result verification failed at element " << i
                      << ": input=" << input_vector[i]
                      << " gpu=" << output_vector[i]
                      << " cpu=" << cpu_result << "!" << std::endl;
            return EXIT_FAILURE;
        }
    }
    double t = w.elapsed();

    std::cout << "Test PASSED, normal calculation time: " << t << "s" << std::endl;
    std::cout << "Done\n";

    return 0;
}

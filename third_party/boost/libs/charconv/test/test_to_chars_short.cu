//  Copyright Matt Borland 2024 - 2026.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <iomanip>
#include <vector>
#include <random>
#include <limits>
#include <cstring>
#include <boost/charconv/to_chars.hpp>
#include "cuda_managed_ptr.hpp"
#include "stopwatch.hpp"

// For the CUDA runtime routines (prefixed with "cuda_")
#include <cuda_runtime.h>

using test_type = short;

constexpr int BUF_SIZE = 32;

__global__ void cuda_test(const test_type *in, char *out_strings, int *out_lengths, int numElements)
{
    int i = blockDim.x * blockIdx.x + threadIdx.x;

    if (i < numElements)
    {
        char* buf = out_strings + i * BUF_SIZE;
        auto res = boost::charconv::to_chars(buf, buf + BUF_SIZE, in[i]);
        out_lengths[i] = static_cast<int>(res.ptr - buf);
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

    // Allocate the managed output vectors
    cuda_managed_ptr<char> output_strings(numElements * BUF_SIZE);
    cuda_managed_ptr<int> output_lengths(numElements);

    // Initialize the input vectors
    std::uniform_int_distribution<short> dist {(std::numeric_limits<test_type>::min)(), (std::numeric_limits<test_type>::max)()};
    for (std::size_t i = 0; i < numElements; ++i)
    {
        input_vector[i] = dist(rng);
    }

    // Launch the CUDA Kernel
    int threadsPerBlock = 256;
    int blocksPerGrid =(numElements + threadsPerBlock - 1) / threadsPerBlock;
    std::cout << "CUDA kernel launch with " << blocksPerGrid << " blocks of " << threadsPerBlock << " threads" << std::endl;

    watch w;

    cuda_test<<<blocksPerGrid, threadsPerBlock>>>(input_vector.get(), output_strings.get(), output_lengths.get(), numElements);
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
        char cpu_buf[BUF_SIZE];
        auto cpu_res = boost::charconv::to_chars(cpu_buf, cpu_buf + BUF_SIZE, input_vector[i]);
        int cpu_len = static_cast<int>(cpu_res.ptr - cpu_buf);
        int gpu_len = output_lengths[i];
        const char* gpu_buf = &output_strings[i * BUF_SIZE];

        if (cpu_len != gpu_len || std::memcmp(cpu_buf, gpu_buf, static_cast<std::size_t>(cpu_len)) != 0)
        {
            std::cerr << "Result verification failed at element " << i << "!" << std::endl;
            return EXIT_FAILURE;
        }
    }
    double t = w.elapsed();

    std::cout << "Test PASSED, normal calculation time: " << t << "s" << std::endl;
    std::cout << "Done\n";

    return 0;
}

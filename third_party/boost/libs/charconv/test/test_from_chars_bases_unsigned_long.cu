//  Copyright Matt Borland 2024 - 2026.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <iomanip>
#include <vector>
#include <random>
#include <limits>
#include <boost/charconv/from_chars.hpp>
#include <boost/charconv/to_chars.hpp>
#include "cuda_managed_ptr.hpp"
#include "stopwatch.hpp"

// For the CUDA runtime routines (prefixed with "cuda_")
#include <cuda_runtime.h>

using test_type = unsigned long;

constexpr int BUF_SIZE = 128;

__global__ void cuda_test(const char *in_strings, const int *in_lengths, test_type *out, int numElements, int base)
{
    int i = blockDim.x * blockIdx.x + threadIdx.x;

    if (i < numElements)
    {
        const char* str = in_strings + i * BUF_SIZE;
        test_type val {};
        boost::charconv::from_chars(str, str + in_lengths[i], val, base);
        out[i] = val;
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
    cuda_managed_ptr<char> input_strings(numElements * BUF_SIZE);
    cuda_managed_ptr<int> input_lengths(numElements);

    // Allocate the managed output vector
    cuda_managed_ptr<test_type> output_vector(numElements);

    int threadsPerBlock = 256;
    int blocksPerGrid = (numElements + threadsPerBlock - 1) / threadsPerBlock;

    std::uniform_int_distribution<unsigned long> dist {(std::numeric_limits<test_type>::min)(), (std::numeric_limits<test_type>::max)()};
    std::vector<test_type> expected(numElements);

    for (int base = 2; base <= 36; ++base)
    {
        // Initialize the input vectors
        for (std::size_t i = 0; i < numElements; ++i)
        {
            expected[i] = dist(rng);
            char* buf = &input_strings[i * BUF_SIZE];
            auto res = boost::charconv::to_chars(buf, buf + BUF_SIZE, expected[i], base);
            input_lengths[i] = static_cast<int>(res.ptr - buf);
        }

        // Launch the CUDA Kernel
        std::cout << "CUDA kernel launch with " << blocksPerGrid << " blocks of " << threadsPerBlock << " threads, base " << base << std::endl;

        watch w;

        cuda_test<<<blocksPerGrid, threadsPerBlock>>>(input_strings.get(), input_lengths.get(), output_vector.get(), numElements, base);
        cudaDeviceSynchronize();

        std::cout << "CUDA kernal done in: " << w.elapsed() << "s" << std::endl;

        err = cudaGetLastError();

        if (err != cudaSuccess)
        {
            std::cerr << "Failed to launch kernel (error code " << cudaGetErrorString(err) << ")!" << std::endl;
            return EXIT_FAILURE;
        }

        // Verify that the result vector is correct
        std::vector<test_type> results;
        results.reserve(numElements);
        w.reset();
        for(int i = 0; i < numElements; ++i)
        {
            test_type val {};
            const char* str = &input_strings[i * BUF_SIZE];
            boost::charconv::from_chars(str, str + input_lengths[i], val, base);
            results.push_back(val);
        }
        double t = w.elapsed();
        // check the results
        for(int i = 0; i < numElements; ++i)
        {
            if (output_vector[i] != results[i])
            {
                std::cerr << "Result verification failed at element " << i << " base " << base << "!" << std::endl;
                return EXIT_FAILURE;
            }
        }

        std::cout << "Test base " << base << " PASSED, normal calculation time: " << t << "s" << std::endl;
    }

    std::cout << "All bases PASSED" << std::endl;
    std::cout << "Done\n";

    return 0;
}

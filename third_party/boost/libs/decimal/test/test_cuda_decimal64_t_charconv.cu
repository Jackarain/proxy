//  Copyright Matt Borland 2026.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <vector>
#include <random>
#include <boost/decimal/decimal64_t.hpp>
#include <boost/decimal/charconv.hpp>
#include "cuda_managed_ptr.hpp"
#include "stopwatch.hpp"

#include <cuda_runtime.h>

using test_type = boost::decimal::decimal64_t;

__global__ void cuda_test(const test_type *in, test_type *out, int numElements)
{
    int i = blockDim.x * blockIdx.x + threadIdx.x;

    if (i < numElements)
    {
        char buffer[64] {};
        auto r = boost::decimal::to_chars(buffer, buffer + sizeof(buffer), in[i]);

        test_type roundtrip {};
        boost::decimal::from_chars(buffer, r.ptr, roundtrip);
        out[i] = roundtrip;
    }
}

int main(void)
{
    std::mt19937_64 rng{42};

    int numElements = 50000;
    std::cout << "[Vector operation on " << numElements << " elements]" << std::endl;

    cuda_managed_ptr<test_type> input_vector(numElements);
    cuda_managed_ptr<test_type> output_vector(numElements);

    std::uniform_int_distribution<int> dist{1, 9999999};
    for (int i = 0; i < numElements; ++i)
    {
        input_vector[i] = test_type(dist(rng));
    }

    int threadsPerBlock = 256;
    int blocksPerGrid = (numElements + threadsPerBlock - 1) / threadsPerBlock;
    std::cout << "CUDA kernel launch with " << blocksPerGrid << " blocks of " << threadsPerBlock << " threads" << std::endl;

    watch w;

    cudaDeviceSetLimit(cudaLimitStackSize, 2048);
    cuda_test<<<blocksPerGrid, threadsPerBlock>>>(input_vector.get(), output_vector.get(), numElements);
    cudaDeviceSynchronize();

    std::cout << "CUDA kernel done in: " << w.elapsed() << "s" << std::endl;

    std::vector<test_type> results;
    results.reserve(numElements);
    w.reset();
    for (int i = 0; i < numElements; ++i)
    {
        char buffer[64] {};
        auto r = boost::decimal::to_chars(buffer, buffer + sizeof(buffer), input_vector[i]);

        test_type roundtrip {};
        boost::decimal::from_chars(buffer, r.ptr, roundtrip);
        results.push_back(roundtrip);
    }
    double t = w.elapsed();

    for (int i = 0; i < numElements; ++i)
    {
        if (output_vector[i] != results[i])
        {
            std::cerr << "Result verification failed at element " << i << "!" << std::endl;
            return EXIT_FAILURE;
        }
    }

    std::cout << "Test PASSED, normal calculation time: " << t << "s" << std::endl;
    std::cout << "Done\n";

    return 0;
}

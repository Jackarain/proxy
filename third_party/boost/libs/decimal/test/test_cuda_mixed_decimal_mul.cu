//  Copyright Matt Borland 2026.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <vector>
#include <random>
#include <boost/decimal/decimal32_t.hpp>
#include <boost/decimal/decimal64_t.hpp>
#include <boost/decimal/decimal128_t.hpp>
#include "cuda_managed_ptr.hpp"
#include "stopwatch.hpp"

#include <cuda_runtime.h>

using dec32_t = boost::decimal::decimal32_t;
using dec64_t = boost::decimal::decimal64_t;
using dec128_t = boost::decimal::decimal128_t;

__global__ void cuda_test(const dec32_t *in32, const dec64_t *in64, const dec128_t *in128,
                          dec64_t *out_32_64, dec128_t *out_32_128, dec128_t *out_64_128,
                          int numElements)
{
    int i = blockDim.x * blockIdx.x + threadIdx.x;

    if (i < numElements)
    {
        out_32_64[i] = in32[i] * in64[i];
        out_32_128[i] = in32[i] * in128[i];
        out_64_128[i] = in64[i] * in128[i];
    }
}

int main(void)
{
    std::mt19937_64 rng{42};

    int numElements = 50000;
    std::cout << "[Mixed decimal multiplication on " << numElements << " elements]" << std::endl;

    cuda_managed_ptr<dec32_t> in32(numElements);
    cuda_managed_ptr<dec64_t> in64(numElements);
    cuda_managed_ptr<dec128_t> in128(numElements);
    cuda_managed_ptr<dec64_t> out_32_64(numElements);
    cuda_managed_ptr<dec128_t> out_32_128(numElements);
    cuda_managed_ptr<dec128_t> out_64_128(numElements);

    std::uniform_int_distribution<int> dist{1, 4999};
    for (int i = 0; i < numElements; ++i)
    {
        in32[i] = dec32_t(dist(rng));
        in64[i] = dec64_t(dist(rng));
        in128[i] = dec128_t(dist(rng));
    }

    int threadsPerBlock = 256;
    int blocksPerGrid = (numElements + threadsPerBlock - 1) / threadsPerBlock;
    std::cout << "CUDA kernel launch with " << blocksPerGrid << " blocks of " << threadsPerBlock << " threads" << std::endl;

    watch w;

    cudaDeviceSetLimit(cudaLimitStackSize, 8192);
    cuda_test<<<blocksPerGrid, threadsPerBlock>>>(in32.get(), in64.get(), in128.get(),
                                                   out_32_64.get(), out_32_128.get(), out_64_128.get(),
                                                   numElements);
    cudaDeviceSynchronize();

    std::cout << "CUDA kernel done in: " << w.elapsed() << "s" << std::endl;

    std::vector<dec64_t> res_32_64;
    std::vector<dec128_t> res_32_128;
    std::vector<dec128_t> res_64_128;
    res_32_64.reserve(numElements);
    res_32_128.reserve(numElements);
    res_64_128.reserve(numElements);

    w.reset();
    for (int i = 0; i < numElements; ++i)
    {
        res_32_64.push_back(in32[i] * in64[i]);
        res_32_128.push_back(in32[i] * in128[i]);
        res_64_128.push_back(in64[i] * in128[i]);
    }
    double t = w.elapsed();

    for (int i = 0; i < numElements; ++i)
    {
        if (out_32_64[i] != res_32_64[i])
        {
            std::cerr << "dec32*dec64 verification failed at element " << i << "!" << std::endl;
            return EXIT_FAILURE;
        }
        if (out_32_128[i] != res_32_128[i])
        {
            std::cerr << "dec32*dec128 verification failed at element " << i << "!" << std::endl;
            return EXIT_FAILURE;
        }
        if (out_64_128[i] != res_64_128[i])
        {
            std::cerr << "dec64*dec128 verification failed at element " << i << "!" << std::endl;
            return EXIT_FAILURE;
        }
    }

    std::cout << "Test PASSED, normal calculation time: " << t << "s" << std::endl;
    std::cout << "Done\n";

    return 0;
}

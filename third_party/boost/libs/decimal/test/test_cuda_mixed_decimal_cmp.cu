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

struct cmp_result
{
    bool eq;
    bool neq;
    bool lt;
    bool le;
    bool gt;
    bool ge;
};

__global__ void cuda_test(const dec32_t *in32_lhs, const dec32_t *in32_rhs,
                          const dec64_t *in64_lhs, const dec64_t *in64_rhs,
                          const dec128_t *in128_lhs, const dec128_t *in128_rhs,
                          cmp_result *out_32_64, cmp_result *out_32_128, cmp_result *out_64_128,
                          int numElements)
{
    int i = blockDim.x * blockIdx.x + threadIdx.x;

    if (i < numElements)
    {
        // dec32 vs dec64
        out_32_64[i].eq  = (in32_lhs[i] == in64_lhs[i]);
        out_32_64[i].neq = (in32_lhs[i] != in64_lhs[i]);
        out_32_64[i].lt  = (in32_lhs[i] <  in64_lhs[i]);
        out_32_64[i].le  = (in32_lhs[i] <= in64_lhs[i]);
        out_32_64[i].gt  = (in32_lhs[i] >  in64_lhs[i]);
        out_32_64[i].ge  = (in32_lhs[i] >= in64_lhs[i]);

        // dec32 vs dec128
        out_32_128[i].eq  = (in32_rhs[i] == in128_lhs[i]);
        out_32_128[i].neq = (in32_rhs[i] != in128_lhs[i]);
        out_32_128[i].lt  = (in32_rhs[i] <  in128_lhs[i]);
        out_32_128[i].le  = (in32_rhs[i] <= in128_lhs[i]);
        out_32_128[i].gt  = (in32_rhs[i] >  in128_lhs[i]);
        out_32_128[i].ge  = (in32_rhs[i] >= in128_lhs[i]);

        // dec64 vs dec128
        out_64_128[i].eq  = (in64_rhs[i] == in128_rhs[i]);
        out_64_128[i].neq = (in64_rhs[i] != in128_rhs[i]);
        out_64_128[i].lt  = (in64_rhs[i] <  in128_rhs[i]);
        out_64_128[i].le  = (in64_rhs[i] <= in128_rhs[i]);
        out_64_128[i].gt  = (in64_rhs[i] >  in128_rhs[i]);
        out_64_128[i].ge  = (in64_rhs[i] >= in128_rhs[i]);
    }
}

int main(void)
{
    std::mt19937_64 rng{42};

    int numElements = 50000;
    std::cout << "[Mixed decimal comparison on " << numElements << " elements]" << std::endl;

    cuda_managed_ptr<dec32_t> in32_lhs(numElements);
    cuda_managed_ptr<dec32_t> in32_rhs(numElements);
    cuda_managed_ptr<dec64_t> in64_lhs(numElements);
    cuda_managed_ptr<dec64_t> in64_rhs(numElements);
    cuda_managed_ptr<dec128_t> in128_lhs(numElements);
    cuda_managed_ptr<dec128_t> in128_rhs(numElements);
    cuda_managed_ptr<cmp_result> out_32_64(numElements);
    cuda_managed_ptr<cmp_result> out_32_128(numElements);
    cuda_managed_ptr<cmp_result> out_64_128(numElements);

    std::uniform_int_distribution<int> dist{1, 4999};
    for (int i = 0; i < numElements; ++i)
    {
        in32_lhs[i] = dec32_t(dist(rng));
        in32_rhs[i] = dec32_t(dist(rng));
        in64_lhs[i] = dec64_t(dist(rng));
        in64_rhs[i] = dec64_t(dist(rng));
        in128_lhs[i] = dec128_t(dist(rng));
        in128_rhs[i] = dec128_t(dist(rng));
    }

    int threadsPerBlock = 256;
    int blocksPerGrid = (numElements + threadsPerBlock - 1) / threadsPerBlock;
    std::cout << "CUDA kernel launch with " << blocksPerGrid << " blocks of " << threadsPerBlock << " threads" << std::endl;

    watch w;

    cudaDeviceSetLimit(cudaLimitStackSize, 8192);
    cuda_test<<<blocksPerGrid, threadsPerBlock>>>(in32_lhs.get(), in32_rhs.get(),
                                                   in64_lhs.get(), in64_rhs.get(),
                                                   in128_lhs.get(), in128_rhs.get(),
                                                   out_32_64.get(), out_32_128.get(), out_64_128.get(),
                                                   numElements);
    cudaDeviceSynchronize();

    std::cout << "CUDA kernel done in: " << w.elapsed() << "s" << std::endl;

    w.reset();
    for (int i = 0; i < numElements; ++i)
    {
        // dec32 vs dec64
        {
            bool eq  = (in32_lhs[i] == in64_lhs[i]);
            bool neq = (in32_lhs[i] != in64_lhs[i]);
            bool lt  = (in32_lhs[i] <  in64_lhs[i]);
            bool le  = (in32_lhs[i] <= in64_lhs[i]);
            bool gt  = (in32_lhs[i] >  in64_lhs[i]);
            bool ge  = (in32_lhs[i] >= in64_lhs[i]);

            if (out_32_64[i].eq != eq || out_32_64[i].neq != neq ||
                out_32_64[i].lt != lt || out_32_64[i].le != le ||
                out_32_64[i].gt != gt || out_32_64[i].ge != ge)
            {
                std::cerr << "dec32 vs dec64 comparison failed at element " << i << "!" << std::endl;
                return EXIT_FAILURE;
            }
        }

        // dec32 vs dec128
        {
            bool eq  = (in32_rhs[i] == in128_lhs[i]);
            bool neq = (in32_rhs[i] != in128_lhs[i]);
            bool lt  = (in32_rhs[i] <  in128_lhs[i]);
            bool le  = (in32_rhs[i] <= in128_lhs[i]);
            bool gt  = (in32_rhs[i] >  in128_lhs[i]);
            bool ge  = (in32_rhs[i] >= in128_lhs[i]);

            if (out_32_128[i].eq != eq || out_32_128[i].neq != neq ||
                out_32_128[i].lt != lt || out_32_128[i].le != le ||
                out_32_128[i].gt != gt || out_32_128[i].ge != ge)
            {
                std::cerr << "dec32 vs dec128 comparison failed at element " << i << "!" << std::endl;
                return EXIT_FAILURE;
            }
        }

        // dec64 vs dec128
        {
            bool eq  = (in64_rhs[i] == in128_rhs[i]);
            bool neq = (in64_rhs[i] != in128_rhs[i]);
            bool lt  = (in64_rhs[i] <  in128_rhs[i]);
            bool le  = (in64_rhs[i] <= in128_rhs[i]);
            bool gt  = (in64_rhs[i] >  in128_rhs[i]);
            bool ge  = (in64_rhs[i] >= in128_rhs[i]);

            if (out_64_128[i].eq != eq || out_64_128[i].neq != neq ||
                out_64_128[i].lt != lt || out_64_128[i].le != le ||
                out_64_128[i].gt != gt || out_64_128[i].ge != ge)
            {
                std::cerr << "dec64 vs dec128 comparison failed at element " << i << "!" << std::endl;
                return EXIT_FAILURE;
            }
        }
    }
    double t = w.elapsed();

    std::cout << "Test PASSED, normal calculation time: " << t << "s" << std::endl;
    std::cout << "Done\n";

    return 0;
}

//  Copyright Matt Borland 2026.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <boost/decimal/decimal32_t.hpp>
#include <boost/decimal/numbers.hpp>
#include "cuda_managed_ptr.hpp"
#include "stopwatch.hpp"

#include <cuda_runtime.h>

using test_type = boost::decimal::decimal32_t;

constexpr int NUM_CONSTANTS = 19;

__global__ void cuda_test(test_type *out)
{
    int i = blockDim.x * blockIdx.x + threadIdx.x;

    if (i == 0)  out[0]  = boost::decimal::numbers::e_v<test_type>;
    if (i == 1)  out[1]  = boost::decimal::numbers::log2e_v<test_type>;
    if (i == 2)  out[2]  = boost::decimal::numbers::log10e_v<test_type>;
    if (i == 3)  out[3]  = boost::decimal::numbers::log10_2_v<test_type>;
    if (i == 4)  out[4]  = boost::decimal::numbers::pi_v<test_type>;
    if (i == 5)  out[5]  = boost::decimal::numbers::pi_over_four_v<test_type>;
    if (i == 6)  out[6]  = boost::decimal::numbers::inv_pi_v<test_type>;
    if (i == 7)  out[7]  = boost::decimal::numbers::inv_sqrtpi_v<test_type>;
    if (i == 8)  out[8]  = boost::decimal::numbers::ln2_v<test_type>;
    if (i == 9)  out[9]  = boost::decimal::numbers::ln10_v<test_type>;
    if (i == 10) out[10] = boost::decimal::numbers::sqrt2_v<test_type>;
    if (i == 11) out[11] = boost::decimal::numbers::sqrt3_v<test_type>;
    if (i == 12) out[12] = boost::decimal::numbers::sqrt10_v<test_type>;
    if (i == 13) out[13] = boost::decimal::numbers::cbrt2_v<test_type>;
    if (i == 14) out[14] = boost::decimal::numbers::cbrt10_v<test_type>;
    if (i == 15) out[15] = boost::decimal::numbers::inv_sqrt2_v<test_type>;
    if (i == 16) out[16] = boost::decimal::numbers::inv_sqrt3_v<test_type>;
    if (i == 17) out[17] = boost::decimal::numbers::egamma_v<test_type>;
    if (i == 18) out[18] = boost::decimal::numbers::phi_v<test_type>;
}

int main(void)
{
    std::cout << "[Testing numeric constants on device for decimal32_t]" << std::endl;

    cuda_managed_ptr<test_type> output_vector(NUM_CONSTANTS);

    int threadsPerBlock = 256;
    int blocksPerGrid = 1;
    std::cout << "CUDA kernel launch with " << blocksPerGrid << " blocks of " << threadsPerBlock << " threads" << std::endl;

    watch w;

    cuda_test<<<blocksPerGrid, threadsPerBlock>>>(output_vector.get());
    cudaDeviceSynchronize();

    std::cout << "CUDA kernel done in: " << w.elapsed() << "s" << std::endl;

    const test_type expected[] = {
        boost::decimal::numbers::e_v<test_type>,
        boost::decimal::numbers::log2e_v<test_type>,
        boost::decimal::numbers::log10e_v<test_type>,
        boost::decimal::numbers::log10_2_v<test_type>,
        boost::decimal::numbers::pi_v<test_type>,
        boost::decimal::numbers::pi_over_four_v<test_type>,
        boost::decimal::numbers::inv_pi_v<test_type>,
        boost::decimal::numbers::inv_sqrtpi_v<test_type>,
        boost::decimal::numbers::ln2_v<test_type>,
        boost::decimal::numbers::ln10_v<test_type>,
        boost::decimal::numbers::sqrt2_v<test_type>,
        boost::decimal::numbers::sqrt3_v<test_type>,
        boost::decimal::numbers::sqrt10_v<test_type>,
        boost::decimal::numbers::cbrt2_v<test_type>,
        boost::decimal::numbers::cbrt10_v<test_type>,
        boost::decimal::numbers::inv_sqrt2_v<test_type>,
        boost::decimal::numbers::inv_sqrt3_v<test_type>,
        boost::decimal::numbers::egamma_v<test_type>,
        boost::decimal::numbers::phi_v<test_type>,
    };

    const char* names[] = {
        "e", "log2e", "log10e", "log10_2", "pi", "pi_over_four",
        "inv_pi", "inv_sqrtpi", "ln2", "ln10", "sqrt2", "sqrt3",
        "sqrt10", "cbrt2", "cbrt10", "inv_sqrt2", "inv_sqrt3",
        "egamma", "phi"
    };

    for (int i = 0; i < NUM_CONSTANTS; ++i)
    {
        if (output_vector[i] != expected[i])
        {
            std::cerr << "Result verification failed for " << names[i] << " at index " << i << "!" << std::endl;
            return EXIT_FAILURE;
        }
    }

    std::cout << "Test PASSED" << std::endl;
    std::cout << "Done\n";

    return 0;
}

// Copyright 2026 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// This example demonstrates using a decimal type and its operations inside a CUDA kernel.
// The file must be compiled with NVCC. Defining BOOST_DECIMAL_ENABLE_CUDA before including
// any library header makes the types and many of the functions usable in both host and
// device code.

#define BOOST_DECIMAL_ENABLE_CUDA

#include <boost/decimal/decimal64_t.hpp>
#include <iostream>
#include <vector>
#include <random>
#include <stdexcept>
#include <cstdlib>

#include <cuda_runtime.h>

using test_type = boost::decimal::decimal64_t;

// Adds two vectors of decimal values element-wise on the device
__global__ void cuda_add(const test_type* in1, const test_type* in2, test_type* out, int numElements)
{
    int i = blockDim.x * blockIdx.x + threadIdx.x;

    if (i < numElements)
    {
        out[i] = in1[i] + in2[i];
    }
}

// Allocate managed space so that the array can be used on both host and device
void allocate(test_type** in, int numElements)
{
    cudaError_t err {cudaMallocManaged(in, numElements * sizeof(test_type))};
    if (err != cudaSuccess)
    {
        throw std::runtime_error(cudaGetErrorString(err));
    }

    cudaDeviceSynchronize();
}

void cleanup(test_type** in1, test_type** in2, test_type** out)
{
    if (*in1 != nullptr)
    {
        cudaFree(*in1);
        *in1 = nullptr;
    }

    if (*in2 != nullptr)
    {
        cudaFree(*in2);
        *in2 = nullptr;
    }

    if (*out != nullptr)
    {
        cudaFree(*out);
        *out = nullptr;
    }

    cudaDeviceReset();
}

int main()
{
    std::mt19937_64 rng {42};

    const int numElements {50000};
    std::cout << "[Vector operation on " << numElements << " elements]" << std::endl;

    // Allocate managed space for our inputs and GPU output, then fill the inputs with random values

    test_type* in1 {nullptr};
    test_type* in2 {nullptr};
    test_type* out {nullptr};

    allocate(&in1, numElements);
    allocate(&in2, numElements);
    allocate(&out, numElements);

    std::uniform_int_distribution<int> dist {1, 4999};
    for (int i {}; i < numElements; ++i)
    {
        in1[i] = test_type{dist(rng)};
        in2[i] = test_type{dist(rng)};
    }

    const int threadsPerBlock {256};
    const int blocksPerGrid {(numElements + threadsPerBlock - 1) / threadsPerBlock};
    std::cout << "CUDA kernel launch with " << blocksPerGrid << " blocks of " << threadsPerBlock << " threads" << std::endl;

    // Launch the CUDA kernel and check for errors

    cuda_add<<<blocksPerGrid, threadsPerBlock>>>(in1, in2, out, numElements);
    cudaDeviceSynchronize();

    cudaError_t err {cudaGetLastError()};
    if (err != cudaSuccess)
    {
        std::cerr << "Failed to launch kernel (error code " << cudaGetErrorString(err) << ")!" << std::endl;
        cleanup(&in1, &in2, &out);
        return EXIT_FAILURE;
    }

    // Perform the same operation on the host so that the results can be compared

    std::vector<test_type> results;
    results.reserve(numElements);

    for (int i {}; i < numElements; ++i)
    {
        results.push_back(in1[i] + in2[i]);
    }

    // The decimal results computed on the GPU and the host should be identical

    for (int i {}; i < numElements; ++i)
    {
        if (out[i] != results[i])
        {
            std::cerr << "Result verification failed at element: " << i << "!" << std::endl;
            cleanup(&in1, &in2, &out);
            return EXIT_FAILURE;
        }
    }

    cleanup(&in1, &in2, &out);

    std::cout << "All CPU and GPU computed elements match!" << std::endl;

    return 0;
}

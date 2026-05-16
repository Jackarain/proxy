// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <inttypes.h>

#define K 20000000
#define N 5

double float_rand(double min, double max)
{
    float scale = rand() / (double) RAND_MAX;
    return min + scale * (max - min);
}

__attribute__ ((__noinline__)) void generate_vector_32(_Decimal32* buffer, size_t buffer_len)
{
    size_t i = 0;
    while (i < buffer_len)
    {
        buffer[i] = float_rand(0.0, 1.0);
        ++i;
    }
}

__attribute__ ((__noinline__)) void test_comparisons_32(_Decimal32* data, const char* label)
{
    struct timespec t1, t2;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    
    size_t s = 0;

    for (size_t n = 0; n < N; ++n)
    {
        for (size_t k = 0; k < K - 1; ++k)
        {
            _Decimal32 val1 = data[k];
            _Decimal32 val2 = data[k + 1];

            s += (size_t)(val1 > val2);
            s += (size_t)(val1 >= val2);
            s += (size_t)(val1 < val2);
            s += (size_t)(val1 <= val2);
            s += (size_t)(val1 == val2);
            s += (size_t)(val1 != val2);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t2);

    uint64_t elapsed_time_us = (uint64_t)((t2.tv_sec - t1.tv_sec) * 1000000 + (t2.tv_nsec - t1.tv_nsec) / 1000);
    printf("Comparisons    <%-10s >: %-10" PRIu64 " us (s=%zu)\n", label, elapsed_time_us, s);
}

__attribute__ ((__noinline__)) void generate_vector_64(_Decimal64* buffer, size_t buffer_len)
{
    size_t i = 0;
    while (i < buffer_len)
    {
        buffer[i] = float_rand(0.0, 1.0);
        ++i;
    }
}

__attribute__ ((__noinline__)) void test_comparisons_64(_Decimal64* data, const char* label)
{
    struct timespec t1, t2;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    
    size_t s = 0;

    for (size_t n = 0; n < N; ++n)
    {
        for (size_t k = 0; k < K - 1; ++k)
        {
            _Decimal64 val1 = data[k];
            _Decimal64 val2 = data[k + 1];

            s += (size_t)(val1 > val2);
            s += (size_t)(val1 >= val2);
            s += (size_t)(val1 < val2);
            s += (size_t)(val1 <= val2);
            s += (size_t)(val1 == val2);
            s += (size_t)(val1 != val2);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t2);

    uint64_t elapsed_time_us = (uint64_t)((t2.tv_sec - t1.tv_sec) * 1000000 + (t2.tv_nsec - t1.tv_nsec) / 1000);
    printf("Comparisons    <%-10s >: %-10" PRIu64 " us (s=%zu)\n", label, elapsed_time_us, s);
}

__attribute__ ((__noinline__)) void generate_vector_128(_Decimal128* buffer, size_t buffer_len)
{
    size_t i = 0;
    while (i < buffer_len)
    {
        buffer[i] = float_rand(0.0, 1.0);
        ++i;
    }
}

__attribute__ ((__noinline__)) void test_comparisons_128(_Decimal128* data, const char* label)
{
    struct timespec t1, t2;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    
    size_t s = 0;

    for (size_t n = 0; n < N; ++n)
    {
        for (size_t k = 0; k < K - 1; ++k)
        {
            _Decimal128 val1 = data[k];
            _Decimal128 val2 = data[k + 1];

            s += (size_t)(val1 > val2);
            s += (size_t)(val1 >= val2);
            s += (size_t)(val1 < val2);
            s += (size_t)(val1 <= val2);
            s += (size_t)(val1 == val2);
            s += (size_t)(val1 != val2);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t2);

    uint64_t elapsed_time_us = (uint64_t)((t2.tv_sec - t1.tv_sec) * 1000000 + (t2.tv_nsec - t1.tv_nsec) / 1000);
    printf("Comparisons    <%-10s>: %-10" PRIu64 " us (s=%zu)\n", label, elapsed_time_us, s);
}

typedef _Decimal32 (*operation_32)(_Decimal32, _Decimal32);

__attribute__ ((__noinline__)) _Decimal32 add_32(_Decimal32 a, _Decimal32 b)
{
    return a + b;
}
__attribute__ ((__noinline__)) _Decimal32 sub_32(_Decimal32 a, _Decimal32 b)
{
    return a - b;
}

__attribute__ ((__noinline__)) _Decimal32 mul_32(_Decimal32 a, _Decimal32 b)
{
    return a * b;
}

__attribute__ ((__noinline__)) _Decimal32 div_32(_Decimal32 a, _Decimal32 b)
{
    return a / b;
}

__attribute__ ((__noinline__)) void test_two_element_operation_32(_Decimal32* data, operation_32 op, const char* label, const char* op_label)
{
    struct timespec t1, t2;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    
    size_t s = 0;

    for (size_t n = 0; n < N; ++n)
    {
        for (size_t k = 0; k < K - 1; ++k)
        {
            _Decimal32 val1 = data[k];
            _Decimal32 val2 = data[k + 1];

            s += (size_t)op(val1, val2);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t2);

    uint64_t elapsed_time_us = (uint64_t)((t2.tv_sec - t1.tv_sec) * 1000000 + (t2.tv_nsec - t1.tv_nsec) / 1000);
    printf("%-15s<%-10s >: %-10" PRIu64 " us (s=%zu)\n", op_label, label, elapsed_time_us, s);
}

typedef _Decimal64 (*operation_64)(_Decimal64, _Decimal64);

__attribute__ ((__noinline__)) _Decimal64 add_64(_Decimal64 a, _Decimal64 b)
{
    return a + b;
}

__attribute__ ((__noinline__)) _Decimal64 sub_64(_Decimal64 a, _Decimal64 b)
{
    return a - b;
}

__attribute__ ((__noinline__)) _Decimal64 mul_64(_Decimal64 a, _Decimal64 b)
{
    return a * b;
}

__attribute__ ((__noinline__)) _Decimal64 div_64(_Decimal64 a, _Decimal64 b)
{
    return a / b;
}

__attribute__ ((__noinline__)) void test_two_element_operation_64(_Decimal64* data, operation_64 op, const char* label, const char* op_label)
{
    struct timespec t1, t2;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    
    size_t s = 0;

    for (size_t n = 0; n < N; ++n)
    {
        for (size_t k = 0; k < K - 1; ++k)
        {
            _Decimal64 val1 = data[k];
            _Decimal64 val2 = data[k + 1];

            s += (size_t)op(val1, val2);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t2);

    uint64_t elapsed_time_us = (uint64_t)((t2.tv_sec - t1.tv_sec) * 1000000 + (t2.tv_nsec - t1.tv_nsec) / 1000);
    printf("%-15s<%-10s >: %-10" PRIu64 " us (s=%zu)\n", op_label, label, elapsed_time_us, s);
}

typedef _Decimal128 (*operation_128)(_Decimal128, _Decimal128);

__attribute__ ((__noinline__)) _Decimal128 add_128(_Decimal128 a, _Decimal128 b)
{
    return a + b;
}

__attribute__ ((__noinline__)) _Decimal128 sub_128(_Decimal128 a, _Decimal128 b)
{
    return a - b;
}

__attribute__ ((__noinline__)) _Decimal128 mul_128(_Decimal128 a, _Decimal128 b)
{
    return a * b;
}

__attribute__ ((__noinline__)) _Decimal128 div_128(_Decimal128 a, _Decimal128 b)
{
    return a / b;
}

__attribute__ ((__noinline__)) void test_two_element_operation_128(_Decimal128* data, operation_128 op, const char* label, const char* op_label)
{
    struct timespec t1, t2;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    
    size_t s = 0;

    for (size_t n = 0; n < N; ++n)
    {
        for (size_t k = 0; k < K - 1; ++k)
        {
            _Decimal128 val1 = data[k];
            _Decimal128 val2 = data[k + 1];

            s += (size_t)op(val1, val2);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t2);

    uint64_t elapsed_time_us = (uint64_t)((t2.tv_sec - t1.tv_sec) * 1000000 + (t2.tv_nsec - t1.tv_nsec) / 1000);
    printf("%-15s<%-10s>: %-10" PRIu64 " us (s=%zu)\n", op_label, label, elapsed_time_us, s);
}

int main()
{
    // One time init of random number generator
    srand(time(NULL));
    
    _Decimal32* d32_array = malloc(K * sizeof(_Decimal32));
    _Decimal64* d64_array = malloc(K * sizeof(_Decimal64));
    _Decimal128* d128_array = malloc(K * sizeof(_Decimal128));
    
    if (d32_array == NULL || d64_array == NULL || d128_array == NULL)
    {
        return 1;
    }

    printf("===== Comparisons =====\n");

    generate_vector_32(d32_array, K);
    test_comparisons_32(d32_array, "_Decimal32");

    generate_vector_64(d64_array, K);
    test_comparisons_64(d64_array, "_Decimal64");

    generate_vector_128(d128_array, K);
    test_comparisons_128(d128_array, "_Decimal128");
    
    printf("\n===== Addition =====\n");

    test_two_element_operation_32(d32_array, add_32, "_Decimal32", "Addition");
    test_two_element_operation_64(d64_array, add_64, "_Decimal64", "Addition");
    test_two_element_operation_128(d128_array, add_128, "_Decimal128", "Addition");

    printf("\n===== Subtraction =====\n");

    test_two_element_operation_32(d32_array, sub_32, "_Decimal32", "Subtraction");
    test_two_element_operation_64(d64_array, sub_64, "_Decimal64", "Subtraction");
    test_two_element_operation_128(d128_array, sub_128, "_Decimal128", "Subtraction");

    printf("\n===== Multiplication =====\n");

    test_two_element_operation_32(d32_array, mul_32, "_Decimal32", "Multiplication");
    test_two_element_operation_64(d64_array, mul_64, "_Decimal64", "Multiplication");
    test_two_element_operation_128(d128_array, mul_128, "_Decimal128", "Multiplication");

    printf("\n===== Division =====\n");

    test_two_element_operation_32(d32_array, div_32, "_Decimal32", "Division");
    test_two_element_operation_64(d64_array, div_64, "_Decimal64", "Division");
    test_two_element_operation_128(d128_array, div_128, "_Decimal128", "Division");

    free(d32_array);
    free(d64_array);
    free(d128_array);

    return 0;
}

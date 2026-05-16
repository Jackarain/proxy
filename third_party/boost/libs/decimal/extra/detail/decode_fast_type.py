# Copyright 2025 Matt Borland
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

import sys
import os
sys.path.insert(0, os.path.dirname(__file__))

from generate_string import generate_string

def decode_decimal_fast32(significand, exp, sign):

    # See values of non-finite masks in decimal_fast32_t.hpp
    if significand >= 536870912:
        isnan = False

        if significand == 536870912:
            result = "-INF" if sign else "INF"
        elif significand >= 3758096384:
            result = "-SNAN" if sign else "SNAN"
            significand ^= 3758096384
            isnan = True
        elif significand >= 1610612736:
            result = "-QNAN" if sign else "QNAN"
            significand ^= 1610612736
            isnan = True
        else:
            raise ValueError("Unknown Finite Value")

        if isnan and significand > 0:
            result += '(' + str(significand) + ')'

    else:
        exp -= 101 # Bias value
        result = generate_string(significand, exp, sign)

    return result

def decode_decimal_fast64(significand, exp, sign):

    # See values of non-finite masks in decimal_fast32_t.hpp
    if significand >= 2305843009213693952:
        isnan = False

        if significand == 2305843009213693952:
            result = "-INF" if sign else "INF"
        elif significand >= 16140901064495857664:
            result = "-SNAN" if sign else "SNAN"
            significand ^= 16140901064495857664
            isnan = True
        elif significand >= 6917529027641081856:
            result = "-QNAN" if sign else "QNAN"
            significand ^= 6917529027641081856
            isnan = True
        else:
            raise ValueError("Unknown Finite Value")

        if isnan and significand > 0:
            result += '(' + str(significand) + ')'

    else:
        exp -= 398 # Bias value
        result = generate_string(significand, exp, sign)

    return result

def decode_decimal_fast128(significand, exp, sign):

    high_bits = significand >> 64

    # See values of non-finite masks in decimal_fast32_t.hpp
    if high_bits >= 2305843009213693952:
        isnan = False

        if high_bits == 2305843009213693952:
            result = "-INF" if sign else "INF"
        elif high_bits >= 16140901064495857664:
            result = "-SNAN" if sign else "SNAN"
            significand ^= (16140901064495857664 << 64)
            isnan = True
        elif high_bits >= 6917529027641081856:
            result = "-QNAN" if sign else "QNAN"
            significand ^= (6917529027641081856 << 64)
            isnan = True
        else:
            raise ValueError("Unknown Finite Value")

        if isnan and significand > 0:
            result += '(' + str(significand) + ')'

    else:
        exp -= 6176 # Bias value
        result = generate_string(significand, exp, sign)

    return result

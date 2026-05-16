# Copyright 2025 Matt Borland
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

def generate_string(significand, exp, sign):
    if significand == 0:
        result = "0e+00"
    else:
        sig_str = str(significand)
        n_digits = len(sig_str)

        if n_digits == 1:
            normalized_str = sig_str
            total_exp = exp
        else:
            normalized_str = sig_str[0] + '.' + sig_str[1:]
            total_exp = exp + n_digits - 1

        result = f"{'-' if sign else ''}{normalized_str}e{total_exp:+03d}"

    return result

# Copyright 2025 Matt Borland
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

import sys
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from detail.decode_ieee_type import decode_decimal32
from detail.decode_ieee_type import decode_decimal64
from detail.decode_ieee_type import decode_decimal128
from detail.decode_fast_type import decode_decimal_fast32
from detail.decode_fast_type import decode_decimal_fast64
from detail.decode_fast_type import decode_decimal_fast128

import gdb
import gdb.printing

class Decimal32Printer:
    """Pretty printer for decimal32_t type"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            bits = int(self.val['bits_'])
            return decode_decimal32(bits)
        except Exception as e:
            return f"<invalid decimal32_t: {e}>"

    def children(self):
        yield ('bits_', self.val['bits_'])


class Decimal64Printer:
    """Pretty printer for decimal64_t type"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            bits = int(self.val['bits_'])
            return decode_decimal64(bits)
        except Exception as e:
            return f"<invalid decimal64_t: {e}>"

    def children(self):
        yield ('bits_', self.val['bits_'])


class Decimal128Printer:
    """Pretty printer for decimal128_t type"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            bits = self.val['bits_']
            bits_high = int(bits['high'])
            bits_low = int(bits['low'])
            combined_bits = (bits_high << 64) | bits_low
            return decode_decimal128(combined_bits)
        except Exception as e:
            return f"<invalid decimal128_t: {e}>"

    def children(self):
        yield ('bits_', self.val['bits_'])


class DecimalFast32Printer:
    """Pretty printer for decimal_fast32_t type"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            significand = int(self.val['significand_'])
            exp = int(self.val['exponent_'])
            sign = int(self.val['sign_'])
            return decode_decimal_fast32(significand, exp, sign)
        except Exception as e:
            return f"<invalid decimal_fast32_t: {e}>"

    def children(self):
        yield ('significand_', self.val['significand_'])
        yield ('exponent_', self.val['exponent_'])
        yield ('sign_', self.val['sign_'])


class DecimalFast64Printer:
    """Pretty printer for decimal_fast64_t type"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            significand = int(self.val['significand_'])
            exp = int(self.val['exponent_'])
            sign = int(self.val['sign_'])
            return decode_decimal_fast64(significand, exp, sign)
        except Exception as e:
            return f"<invalid decimal_fast64_t: {e}>"

    def children(self):
        yield ('significand_', self.val['significand_'])
        yield ('exponent_', self.val['exponent_'])
        yield ('sign_', self.val['sign_'])


class DecimalFast128Printer:
    """Pretty printer for decimal_fast128_t type"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            significand = self.val['significand_']
            bits_high = int(significand['high'])
            bits_low = int(significand['low'])
            combined_bits = (bits_high << 64) | bits_low

            exp = int(self.val['exponent_'])
            sign = int(self.val['sign_'])

            return decode_decimal_fast128(combined_bits, exp, sign)
        except Exception as e:
            return f"<invalid decimal_fast128_t: {e}>"

    def children(self):
        yield ('significand_', self.val['significand_'])
        yield ('exponent_', self.val['exponent_'])
        yield ('sign_', self.val['sign_'])


class U256Printer:
    """Pretty printer for u256 internal type"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            bytes = self.val['bytes']
            byte0 = int(bytes[3]) & 0xFFFFFFFFFFFFFFFF
            byte1 = int(bytes[2]) & 0xFFFFFFFFFFFFFFFF
            byte2 = int(bytes[1]) & 0xFFFFFFFFFFFFFFFF
            byte3 = int(bytes[0]) & 0xFFFFFFFFFFFFFFFF

            value = (byte0 << 192) | (byte1 << 128) | (byte2 << 64) | byte3
            return f"{value:,}"
        except Exception as e:
            return f"<invalid u256: {e}>"



def build_pretty_printer():
    """Build and return the pretty printer collection"""
    pp = gdb.printing.RegexpCollectionPrettyPrinter("boost_decimal")

    # IEEE types
    pp.add_printer('decimal32_t',
                   r'^(const )?(boost::decimal::)?decimal32_t( &| \*)?$',
                   Decimal32Printer)
    pp.add_printer('decimal64_t',
                   r'^(const )?(boost::decimal::)?decimal64_t( &| \*)?$',
                   Decimal64Printer)
    pp.add_printer('decimal128_t',
                   r'^(const )?(boost::decimal::)?decimal128_t( &| \*)?$',
                   Decimal128Printer)

    # Fast types
    pp.add_printer('decimal_fast32_t',
                   r'^(const )?(boost::decimal::)?decimal_fast32_t( &| \*)?$',
                   DecimalFast32Printer)
    pp.add_printer('decimal_fast64_t',
                   r'^(const )?(boost::decimal::)?decimal_fast64_t( &| \*)?$',
                   DecimalFast64Printer)
    pp.add_printer('decimal_fast128_t',
                   r'^(const )?(boost::decimal::)?decimal_fast128_t( &| \*)?$',
                   DecimalFast128Printer)

    # Debug internal types
    pp.add_printer('u256',
                   r'^(const )?(boost::decimal::detail::)?u256( &| \*)?$',
                   U256Printer)

    return pp


def register_printers(objfile=None):
    gdb.printing.register_pretty_printer(objfile, build_pretty_printer())


# Auto-register when the module is loaded
register_printers()
print("Boost.Decimal pretty printers loaded successfully")

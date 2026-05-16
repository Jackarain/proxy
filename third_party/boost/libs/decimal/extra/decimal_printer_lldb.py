# Copyright 2025 Matt Borland
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

from detail.decode_ieee_type import decode_decimal32
from detail.decode_ieee_type import decode_decimal64
from detail.decode_ieee_type import decode_decimal128
from detail.decode_fast_type import decode_decimal_fast32
from detail.decode_fast_type import decode_decimal_fast64
from detail.decode_fast_type import decode_decimal_fast128

import lldb

def decimal32_summary(valobj, internal_dict):
    """
    Custom summary for decimal32_t type
    Displays in scientific notation with cohort preservation
    """

    try:
        val = valobj.GetNonSyntheticValue()
        bits = val.GetChildMemberWithName("bits_").GetValueAsUnsigned()
        return decode_decimal32(bits)

    except Exception as e:
        return f"<invalid decimal32_t: {e}>"

def decimal64_summary(valobj, internal_dict):
    """
    Custom summary for decimal64_t type
    Displays in scientific notation with cohort preservation
    """

    try:
        val = valobj.GetNonSyntheticValue()
        bits = val.GetChildMemberWithName("bits_").GetValueAsUnsigned()
        return decode_decimal64(bits)

    except Exception as e:
        return f"<invalid decimal64_t: {e}>"

def decimal128_summary(valobj, internal_dict):
    """
    Custom summary for decimal128_t type
    Displays in scientific notation with cohort preservation
    """

    try:
        val = valobj.GetNonSyntheticValue()
        bits = val.GetChildMemberWithName("bits_")
        bits_high = bits.GetChildMemberWithName("high").GetValueAsUnsigned()
        bits_low = bits.GetChildMemberWithName("low").GetValueAsUnsigned()
        combined_bits = (bits_high << 64) | bits_low
        return decode_decimal128(combined_bits)

    except Exception as e:
        return f"<invalid decimal128_t: {e}>"

def decimal_fast32_summary(valobj, internal_dict):
    """
    Custom summary for decimal_fast32_t type
    Displays in scientific notation
    """

    try:
        val = valobj.GetNonSyntheticValue()
        significand = val.GetChildMemberWithName("significand_").GetValueAsUnsigned()
        exp = val.GetChildMemberWithName("exponent_").GetValueAsUnsigned()
        sign = val.GetChildMemberWithName("sign_").GetValueAsUnsigned()
        return decode_decimal_fast32(significand, exp, sign)

    except Exception as e:
        return f"<invalid decimal_fast32_t: {e}>"

def decimal_fast64_summary(valobj, internal_dict):
    """
    Custom summary for decimal_fast64_t type
    Displays in scientific notation
    """

    try:
        val = valobj.GetNonSyntheticValue()
        significand = val.GetChildMemberWithName("significand_").GetValueAsUnsigned()
        exp = val.GetChildMemberWithName("exponent_").GetValueAsUnsigned()
        sign = val.GetChildMemberWithName("sign_").GetValueAsUnsigned()
        return decode_decimal_fast64(significand, exp, sign)

    except Exception as e:
        return f"<invalid decimal_fast64_t: {e}>"
def decimal_fast128_summary(valobj, internal_dict):
    """
    Custom summary for decimal_fast128_t type
    Displays in scientific notation
    """

    try:
        val = valobj.GetNonSyntheticValue()

        significand = val.GetChildMemberWithName("significand_")
        bits_high = significand.GetChildMemberWithName("high").GetValueAsUnsigned()
        bits_low = significand.GetChildMemberWithName("low").GetValueAsUnsigned()
        combined_bits = (bits_high << 64) | bits_low

        exp = val.GetChildMemberWithName("exponent_").GetValueAsUnsigned()
        sign = val.GetChildMemberWithName("sign_").GetValueAsUnsigned()

        return decode_decimal_fast128(combined_bits, exp, sign)

    except Exception as e:
        return f"<invalid decimal_fast128_t: {e}>"

def u256_summary(valobj, internal_dict):
    """
    Custom summary for u256 detail type
    Displays in decimal notation
    """

    try:
        val = valobj.GetNonSyntheticValue()

        bytes = val.GetChildMemberWithName("bytes")
        b0 = bytes.GetChildAtIndex(0).GetValueAsUnsigned()
        b1 = bytes.GetChildAtIndex(1).GetValueAsUnsigned()
        b2 = bytes.GetChildAtIndex(2).GetValueAsUnsigned()
        b3 = bytes.GetChildAtIndex(3).GetValueAsUnsigned()

        value = (b3 << 192) | (b2 << 128) | (b1 << 64) | b0
        return f"{value:,}"
    except Exception as e:
        return f"<invalid u256: {e}>"

def __lldb_init_module(debugger, internal_dict):
    decimal32_pattern = r"^(const )?(boost::decimal::decimal32_t|(\w+::)*decimal32_t)( &| \*)?$"
    decimal64_pattern = r"^(const )?(boost::decimal::decimal64_t|(\w+::)*decimal64_t)( &| \*)?$"
    decimal128_pattern = r"^(const )?(boost::decimal::decimal128_t|(\w+::)*decimal128_t)( &| \*)?$"

    decimal_fast32_pattern = r"^(const )?(boost::decimal::decimal_fast32_t|(\w+::)*decimal_fast32_t)( &| \*)?$"
    decimal_fast64_pattern = r"^(const )?(boost::decimal::decimal_fast64_t|(\w+::)*decimal_fast64_t)( &| \*)?$"
    decimal_fast128_pattern = r"^(const )?(boost::decimal::decimal_fast128_t|(\w+::)*decimal_fast128_t)( &| \*)?$"

    u256_pattern = r"^(const )?(boost::decimal::detail::u256|(\w+::)*u256)( &| \*)?$"

    debugger.HandleCommand(
        f'type summary add -x "{decimal32_pattern}" -e -F decimal_printer_lldb.decimal32_summary'
    )
    debugger.HandleCommand(
        f'type synthetic add -x "{decimal32_pattern}" -l decimal_printer_lldb.DecimalSyntheticProvider'
    )

    print("decimal32_t printer loaded successfully")

    debugger.HandleCommand(
        f'type summary add -x "{decimal64_pattern}" -e -F decimal_printer_lldb.decimal64_summary'
    )
    debugger.HandleCommand(
        f'type synthetic add -x "{decimal64_pattern}" -l decimal_printer_lldb.DecimalSyntheticProvider'
    )

    print("decimal64_t printer loaded successfully")

    debugger.HandleCommand(
        f'type summary add -x "{decimal128_pattern}" -e -F decimal_printer_lldb.decimal128_summary'
    )
    debugger.HandleCommand(
        f'type synthetic add -x "{decimal128_pattern}" -l decimal_printer_lldb.DecimalSyntheticProvider'
    )

    print("decimal128_t printer loaded successfully")

    debugger.HandleCommand(
        f'type summary add -x "{decimal_fast32_pattern}" -e -F decimal_printer_lldb.decimal_fast32_summary'
    )
    debugger.HandleCommand(
        f'type synthetic add -x "{decimal_fast32_pattern}" -l decimal_printer_lldb.DecimalFastSyntheticProvider'
    )

    print("decimal_fast32_t printer loaded successfully")

    debugger.HandleCommand(
        f'type summary add -x "{decimal_fast64_pattern}" -e -F decimal_printer_lldb.decimal_fast64_summary'
    )
    debugger.HandleCommand(
        f'type synthetic add -x "{decimal_fast64_pattern}" -l decimal_printer_lldb.DecimalFastSyntheticProvider'
    )

    print("decimal_fast64_t printer loaded successfully")

    debugger.HandleCommand(
        f'type summary add -x "{decimal_fast128_pattern}" -e -F decimal_printer_lldb.decimal_fast128_summary'
    )
    debugger.HandleCommand(
        f'type synthetic add -x "{decimal_fast128_pattern}" -l decimal_printer_lldb.DecimalFastSyntheticProvider'
    )

    print("decimal_fast128_t printer loaded successfully")

    debugger.HandleCommand(
        f'type summary add -x "{u256_pattern}" -e -F decimal_printer_lldb.u256_summary'
    )
    debugger.HandleCommand(
        f'type synthetic add -x "{u256_pattern}" -l decimal_printer_lldb.u256SyntheticProvider'
    )

class DecimalSyntheticProvider:
    def __init__(self, valobj, internal_dict):
        self.valobj = valobj

    def num_children(self):
        return 1

    def get_child_index(self, name):
        if name == "bits_":
            return 0
        return -1

    def get_child_at_index(self, index):
        if index == 0:
            return self.valobj.GetChildMemberWithName("bits_")
        return None

    def update(self):
        pass

    def has_children(self):
        return True

class DecimalFastSyntheticProvider:
    def __init__(self, valobj, internal_dict):
        self.valobj = valobj

    def num_children(self):
        return 3

    def get_child_index(self, name):
        if name == "significand_":
            return 0
        elif name == "exponent_":
            return 1
        elif name == "sign_":
            return 2
        else:
            return -1

    def get_child_at_index(self, index):
        if index == 0:
            return self.valobj.GetChildMemberWithName("significand_")
        elif index == 1:
            return self.valobj.GetChildMemberWithName("exponent_")
        elif index == 2:
            return self.valobj.GetChildMemberWithName("sign_")
        else:
            return None

    def update(self):
        pass

    def has_children(self):
        return True

class u256SyntheticProvider:
    def __init__(self, valobj, internal_dict):
        self.valobj = valobj

    def num_children(self):
        return 1

    def get_child_index(self, name):
        if name == "bytes":
            return 0
        return -1

    def get_child_at_index(self, index):
        if index == 0:
            return self.valobj.GetChildMemberWithName("bytes")
        return None

    def update(self):
        pass

    def has_children(self):
        return True

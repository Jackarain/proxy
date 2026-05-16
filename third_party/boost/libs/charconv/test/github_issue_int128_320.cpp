// Copyright 2026 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/cppalliance/int128/issues/320

#include <boost/charconv.hpp>

#ifdef BOOST_CHARCONV_HAS_INT128

#if defined(__GNUC__) && __GNUC__ == 12
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif

#include <boost/core/lightweight_test.hpp>
#include <string>
#include <cstdlib>

void toChars(char* ptr, char* ptrEnd, boost::int128_type i128)
{
    auto r = boost::charconv::to_chars(ptr, ptrEnd, i128);
    *r.ptr = '\0';
}

void toChars(char* ptr, char* ptrEnd, boost::uint128_type u128)
{
    auto r = boost::charconv::to_chars(ptr, ptrEnd, u128);
    *r.ptr = '\0';
}

int main()
{
    boost::int128_type i128 = -123;
    boost::uint128_type u128 = 123;

    std::int64_t ref_value = -123;

    for (int i = 1; i < 5; ++i)
    {
        i128 *= i;
        u128 *= static_cast<unsigned>(i);
        ref_value *= i;

        char buf[64] = {};
        toChars(buf, buf + 64, i128);
        BOOST_TEST_CSTR_EQ(buf, std::to_string(ref_value).c_str());

        toChars(buf, buf + 64, u128);
        BOOST_TEST_CSTR_EQ(buf, std::to_string(std::abs(ref_value)).c_str());
    };

    return boost::report_errors();
}

#else

int main() { return 0; }

#endif

// Copyright 2017, 2018, 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/get_integral_result.hpp>
#include <boost/hash2/digest.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/array.hpp>
#include <cstdint>
#include <array>

template<class R> class H
{
private:

    R r_;

public:

    using result_type = R;

    explicit H( R const& r ): r_( r )
    {
    }

    result_type result()
    {
        return r_;
    }
};

template<class R> void test()
{
    using boost::hash2::get_integral_result;

    {
        R r = {{ 0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01 }};

        {
            H<R> h( r );
            BOOST_TEST_EQ( get_integral_result<std::uint8_t>( h ), 0xEF );
        }

        {
            H<R> h( r );
            BOOST_TEST_EQ( get_integral_result<std::int8_t>( h ), -0x11 );
        }

        {
            H<R> h( r );
            BOOST_TEST_EQ( get_integral_result<std::uint16_t>( h ), 0xCDEF );
        }

        {
            H<R> h( r );
            BOOST_TEST_EQ( get_integral_result<std::int16_t>( h ), -0x3211 );
        }

        {
            H<R> h( r );
            BOOST_TEST_EQ( get_integral_result<std::uint32_t>( h ), 0x89ABCDEF );
        }

        {
            H<R> h( r );
            BOOST_TEST_EQ( get_integral_result<std::int32_t>( h ), 0x89ABCDEF );
        }

        {
            H<R> h( r );
            BOOST_TEST_EQ( get_integral_result<std::uint64_t>( h ), 0x0123456789ABCDEFull );
        }

        {
            H<R> h( r );
            BOOST_TEST_EQ( get_integral_result<std::int64_t>( h ), 0x0123456789ABCDEFull );
        }
    }

    {
        R r = {{ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }};

        {
            H<R> h( r );
            BOOST_TEST_EQ( get_integral_result<std::uint8_t>( h ), 0xFF );
        }

        {
            H<R> h( r );
            BOOST_TEST_EQ( get_integral_result<std::int8_t>( h ), -1 );
        }

        {
            H<R> h( r );
            BOOST_TEST_EQ( get_integral_result<std::uint16_t>( h ), 0xFFFF );
        }

        {
            H<R> h( r );
            BOOST_TEST_EQ( get_integral_result<std::int16_t>( h ), -1 );
        }

        {
            H<R> h( r );
            BOOST_TEST_EQ( get_integral_result<std::uint32_t>( h ), 0xFFFFFFFFu );
        }

        {
            H<R> h( r );
            BOOST_TEST_EQ( get_integral_result<std::int32_t>( h ), -1 );
        }

        {
            H<R> h( r );
            BOOST_TEST_EQ( get_integral_result<std::uint64_t>( h ), 0xFFFFFFFFFFFFFFFFull );
        }

        {
            H<R> h( r );
            BOOST_TEST_EQ( get_integral_result<std::int64_t>( h ), -1 );
        }
    }
}

int main()
{
    test< std::array<unsigned char, 8> >();
    test< boost::array<unsigned char, 8> >();
    test< boost::hash2::digest<8> >();

    return boost::report_errors();
}

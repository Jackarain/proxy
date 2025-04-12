// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/detail/write.hpp>
#include <boost/core/lightweight_test.hpp>
#include <array>

int main()
{
    using namespace boost::hash2::detail;

    {
        std::array<unsigned char, 2> w;

        write16be( w.data(), 0x0102 );

        BOOST_TEST(( w == std::array<unsigned char, 2>{{ 0x01, 0x02 }} ));
    }

    {
        std::array<unsigned char, 2> w;

        write16le( w.data(), 0x0102 );

        BOOST_TEST(( w == std::array<unsigned char, 2>{{ 0x02, 0x01 }} ));
    }

    {
        std::array<unsigned char, 4> w;

        write32be( w.data(), 0x01020304 );

        BOOST_TEST(( w == std::array<unsigned char, 4>{{ 0x01, 0x02, 0x03, 0x04 }} ));
    }

    {
        std::array<unsigned char, 4> w;

        write32le( w.data(), 0x01020304 );

        BOOST_TEST(( w == std::array<unsigned char, 4>{{ 0x04, 0x03, 0x02, 0x01 }} ));
    }

    {
        std::array<unsigned char, 8> w;

        write64be( w.data(), 0x0102030405060708 );

        BOOST_TEST(( w == std::array<unsigned char, 8>{{ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 }} ));
    }

    {
        std::array<unsigned char, 8> w;

        write64le( w.data(), 0x0102030405060708 );

        BOOST_TEST(( w == std::array<unsigned char, 8>{{ 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01 }} ));
    }

    return boost::report_errors();
}

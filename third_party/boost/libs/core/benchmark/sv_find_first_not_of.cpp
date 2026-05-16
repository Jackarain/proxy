// Copyright 2021, 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/utility/string_view.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/type_name.hpp>
#include <boost/cstdint.hpp>
#include <string_view>
#include <chrono>
#include <iostream>

using namespace std::chrono_literals;

template<class Sv> void test()
{
    constexpr char const* q1 = "{";
    constexpr char const* q2 = "<(";
    constexpr char const* q3 = " :=";
    constexpr char const* q4 = " \t\r\n";
    constexpr char const* q6 = " \t\r\n\f\v";
    constexpr char const* q10 = "0123456789";
    constexpr char const* q52 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    constexpr std::size_t npos = static_cast<std::size_t>( -1 );

    constexpr std::size_t N = 1'000'000'000;

    std::cout << boost::core::type_name<Sv>() << ":\n\n";

    auto t0 = std::chrono::steady_clock::now();

    {
        constexpr char const* q = q1;

        std::string s1( 1'000'000, q[ std::strlen( q ) - 1 ] );
        std::string s2( 100, q[ std::strlen( q ) - 1 ] );

        {
            auto t1 = std::chrono::steady_clock::now();

            for( std::size_t i = 0; i < N / s1.size(); ++i )
            {
                BOOST_TEST_EQ( Sv( s1 ).find_first_not_of( q ), npos );
            }

            auto t2 = std::chrono::steady_clock::now();

            std::cout << "find_first_not_of( sv, \"" << q << "\" ) in " << s1.size() << " bytes: " << ( t2 - t1 ) / 1ms << " ms\n";
        }

        {
            auto t1 = std::chrono::steady_clock::now();

            for( std::size_t i = 0; i < N / s2.size(); ++i )
            {
                BOOST_TEST_EQ( Sv( s2 ).find_first_not_of( q ), npos );
            }

            auto t2 = std::chrono::steady_clock::now();

            std::cout << "find_first_not_of( sv, \"" << q << "\" ) in " << s2.size() << " bytes: " << ( t2 - t1 ) / 1ms << " ms\n";
        }
    }

    {
        constexpr char const* q = q2;

        std::string s1( 1'000'000, q[ std::strlen( q ) - 1 ] );
        std::string s2( 100, q[ std::strlen( q ) - 1 ] );

        {
            auto t1 = std::chrono::steady_clock::now();

            for( std::size_t i = 0; i < N / s1.size(); ++i )
            {
                BOOST_TEST_EQ( Sv( s1 ).find_first_not_of( q ), npos );
            }

            auto t2 = std::chrono::steady_clock::now();

            std::cout << "find_first_not_of( sv, \"" << q << "\" ) in " << s1.size() << " bytes: " << ( t2 - t1 ) / 1ms << " ms\n";
        }

        {
            auto t1 = std::chrono::steady_clock::now();

            for( std::size_t i = 0; i < N / s2.size(); ++i )
            {
                BOOST_TEST_EQ( Sv( s2 ).find_first_not_of( q ), npos );
            }

            auto t2 = std::chrono::steady_clock::now();

            std::cout << "find_first_not_of( sv, \"" << q << "\" ) in " << s2.size() << " bytes: " << ( t2 - t1 ) / 1ms << " ms\n";
        }
    }

    {
        constexpr char const* q = q3;

        std::string s1( 1'000'000, q[ std::strlen( q ) - 1 ] );
        std::string s2( 100, q[ std::strlen( q ) - 1 ] );

        {
            auto t1 = std::chrono::steady_clock::now();

            for( std::size_t i = 0; i < N / s1.size(); ++i )
            {
                BOOST_TEST_EQ( Sv( s1 ).find_first_not_of( q ), npos );
            }

            auto t2 = std::chrono::steady_clock::now();

            std::cout << "find_first_not_of( sv, \"" << q << "\" ) in " << s1.size() << " bytes: " << ( t2 - t1 ) / 1ms << " ms\n";
        }

        {
            auto t1 = std::chrono::steady_clock::now();

            for( std::size_t i = 0; i < N / s2.size(); ++i )
            {
                BOOST_TEST_EQ( Sv( s2 ).find_first_not_of( q ), npos );
            }

            auto t2 = std::chrono::steady_clock::now();

            std::cout << "find_first_not_of( sv, \"" << q << "\" ) in " << s2.size() << " bytes: " << ( t2 - t1 ) / 1ms << " ms\n";
        }
    }

    {
        constexpr char const* q = q4;

        std::string s1( 1'000'000, q[ std::strlen( q ) - 1 ] );
        std::string s2( 100, q[ std::strlen( q ) - 1 ] );

        {
            auto t1 = std::chrono::steady_clock::now();

            for( std::size_t i = 0; i < N / s1.size(); ++i )
            {
                BOOST_TEST_EQ( Sv( s1 ).find_first_not_of( q ), npos );
            }

            auto t2 = std::chrono::steady_clock::now();

            std::cout << "find_first_not_of( sv, \"" << q << "\" ) in " << s1.size() << " bytes: " << ( t2 - t1 ) / 1ms << " ms\n";
        }

        {
            auto t1 = std::chrono::steady_clock::now();

            for( std::size_t i = 0; i < N / s2.size(); ++i )
            {
                BOOST_TEST_EQ( Sv( s2 ).find_first_not_of( q ), npos );
            }

            auto t2 = std::chrono::steady_clock::now();

            std::cout << "find_first_not_of( sv, \"" << q << "\" ) in " << s2.size() << " bytes: " << ( t2 - t1 ) / 1ms << " ms\n";
        }
    }

    {
        constexpr char const* q = q6;

        std::string s1( 1'000'000, q[ std::strlen( q ) - 1 ] );
        std::string s2( 100, q[ std::strlen( q ) - 1 ] );

        {
            auto t1 = std::chrono::steady_clock::now();

            for( std::size_t i = 0; i < N / s1.size(); ++i )
            {
                BOOST_TEST_EQ( Sv( s1 ).find_first_not_of( q ), npos );
            }

            auto t2 = std::chrono::steady_clock::now();

            std::cout << "find_first_not_of( sv, \"" << q << "\" ) in " << s1.size() << " bytes: " << ( t2 - t1 ) / 1ms << " ms\n";
        }

        {
            auto t1 = std::chrono::steady_clock::now();

            for( std::size_t i = 0; i < N / s2.size(); ++i )
            {
                BOOST_TEST_EQ( Sv( s2 ).find_first_not_of( q ), npos );
            }

            auto t2 = std::chrono::steady_clock::now();

            std::cout << "find_first_not_of( sv, \"" << q << "\" ) in " << s2.size() << " bytes: " << ( t2 - t1 ) / 1ms << " ms\n";
        }
    }

    {
        constexpr char const* q = q10;

        std::string s1( 1'000'000, q[ std::strlen( q ) - 1 ] );
        std::string s2( 100, q[ std::strlen( q ) - 1 ] );

        {
            auto t1 = std::chrono::steady_clock::now();

            for( std::size_t i = 0; i < N / s1.size(); ++i )
            {
                BOOST_TEST_EQ( Sv( s1 ).find_first_not_of( q ), npos );
            }

            auto t2 = std::chrono::steady_clock::now();

            std::cout << "find_first_not_of( sv, \"" << q << "\" ) in " << s1.size() << " bytes: " << ( t2 - t1 ) / 1ms << " ms\n";
        }

        {
            auto t1 = std::chrono::steady_clock::now();

            for( std::size_t i = 0; i < N / s2.size(); ++i )
            {
                BOOST_TEST_EQ( Sv( s2 ).find_first_not_of( q ), npos );
            }

            auto t2 = std::chrono::steady_clock::now();

            std::cout << "find_first_not_of( sv, \"" << q << "\" ) in " << s2.size() << " bytes: " << ( t2 - t1 ) / 1ms << " ms\n";
        }
    }

    {
        constexpr char const* q = q52;

        std::string s1( 1'000'000, q[ std::strlen( q ) - 1 ] );
        std::string s2( 100, q[ std::strlen( q ) - 1 ] );

        {
            auto t1 = std::chrono::steady_clock::now();

            for( std::size_t i = 0; i < N / s1.size(); ++i )
            {
                BOOST_TEST_EQ( Sv( s1 ).find_first_not_of( q ), npos );
            }

            auto t2 = std::chrono::steady_clock::now();

            std::cout << "find_first_not_of( sv, \"" << q << "\" ) in " << s1.size() << " bytes: " << ( t2 - t1 ) / 1ms << " ms\n";
        }

        {
            auto t1 = std::chrono::steady_clock::now();

            for( std::size_t i = 0; i < N / s2.size(); ++i )
            {
                BOOST_TEST_EQ( Sv( s2 ).find_first_not_of( q ), npos );
            }

            auto t2 = std::chrono::steady_clock::now();

            std::cout << "find_first_not_of( sv, \"" << q << "\" ) in " << s2.size() << " bytes: " << ( t2 - t1 ) / 1ms << " ms\n";
        }
    }

    auto tn = std::chrono::steady_clock::now();

    std::cout << "\nTotal for " << boost::core::type_name<Sv>() << ": " << ( tn - t0 ) / 1ms << " ms\n\n";
}

int main()
{
    test<std::string_view>();
    test<boost::string_view>();
    test<boost::core::string_view>();

    return boost::report_errors();
}

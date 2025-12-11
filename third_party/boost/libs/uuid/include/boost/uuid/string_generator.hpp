#ifndef BOOST_UUID_STRING_GENERATOR_HPP_INCLUDED
#define BOOST_UUID_STRING_GENERATOR_HPP_INCLUDED

// Copyright 2010 Andy Tompkins
// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/uuid/uuid.hpp>
#include <boost/throw_exception.hpp>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>
#include <string>
#include <iterator>
#include <stdexcept>
#include <cstdio>
#include <cstddef>

#if BOOST_WORKAROUND(BOOST_GCC, < 60000)
# define BOOST_UUID_CXX14_CONSTEXPR
#else
# define BOOST_UUID_CXX14_CONSTEXPR BOOST_CXX14_CONSTEXPR
#endif

namespace boost {
namespace uuids {

namespace detail
{

template<class Ch>
BOOST_CXX14_CONSTEXPR std::size_t cx_strlen( Ch const* s )
{
    std::size_t r = 0;
    while( *s ) ++s, ++r;
    return r;
}

template<class Ch>
BOOST_CXX14_CONSTEXPR Ch const* cx_find( Ch const* s, std::size_t n, Ch ch )
{
    for( std::size_t i = 0; i < n; ++i )
    {
        if( s[i] == ch ) return s + i;
    }

    return nullptr;
}

} // namespace detail

// Generates a UUID from a string
//
// Accepts the following forms:
//
// 0123456789abcdef0123456789abcdef
// 01234567-89ab-cdef-0123-456789abcdef
// {01234567-89ab-cdef-0123-456789abcdef}
// {0123456789abcdef0123456789abcdef}

struct string_generator
{
private:

    template <typename CharIterator>
    typename std::iterator_traits<CharIterator>::value_type
    BOOST_UUID_CXX14_CONSTEXPR get_next_char( CharIterator& begin, CharIterator end, int& ipos ) const
    {
        if( begin == end )
        {
            throw_invalid( ipos, "unexpected end of input" );
        }

        ++ipos;
        return *begin++;
    }

public:

    using result_type = uuid;

    template<class CharIterator>
    BOOST_UUID_CXX14_CONSTEXPR uuid operator()( CharIterator begin, CharIterator end ) const
    {
        using char_type = typename std::iterator_traits<CharIterator>::value_type;

        int ipos = 0;

        // check open brace
        char_type c = get_next_char( begin, end, ipos );

        bool has_open_brace = is_open_brace( c );

        if( has_open_brace )
        {
            c = get_next_char( begin, end, ipos );
        }

        bool has_dashes = false;

        uuid u;

        int i = 0;

        for( uuid::iterator it_byte = u.begin(); it_byte != u.end(); ++it_byte, ++i )
        {
            if( it_byte != u.begin() )
            {
                c = get_next_char( begin, end, ipos );
            }

            if( i == 4 )
            {
                has_dashes = is_dash( c );

                if( has_dashes )
                {
                    c = get_next_char( begin, end, ipos );
                }
            }
            else if( i == 6 || i == 8 || i == 10 )
            {
                // if there are dashes, they must be in every slot
                if( has_dashes )
                {
                    if( is_dash( c ) )
                    {
                        c = get_next_char( begin, end, ipos );
                    }
                    else
                    {
                        throw_invalid( ipos - 1, "dash expected" );
                    }
                }
            }

            *it_byte = get_value( c, ipos - 1 );

            c = get_next_char( begin, end, ipos );

            *it_byte <<= 4;
            *it_byte |= get_value( c, ipos - 1 );
        }

        // check close brace
        if( has_open_brace )
        {
            c = get_next_char( begin, end, ipos );

            if( !is_close_brace( c ) )
            {
                throw_invalid( ipos - 1, "closing brace expected" );
            }
        }

        // check end of string - any additional data is an invalid uuid
        if( begin != end )
        {
            throw_invalid( ipos, "unexpected extra input" );
        }

        return u;
    }

    template<class Ch, class Traits, class Alloc>
    BOOST_UUID_CXX14_CONSTEXPR uuid operator()( std::basic_string<Ch, Traits, Alloc> const& s ) const
    {
        return operator()( s.begin(), s.end() );
    }

    BOOST_UUID_CXX14_CONSTEXPR uuid operator()( char const* s ) const
    {
        return operator()( s, s + detail::cx_strlen( s ) );
    }

    BOOST_UUID_CXX14_CONSTEXPR uuid operator()( wchar_t const* s ) const
    {
        return operator()( s, s + detail::cx_strlen( s ) );
    }

private:

    BOOST_NORETURN void throw_invalid( int ipos, char const* error ) const
    {
        char buffer[ 16 ];
        std::snprintf( buffer, sizeof( buffer ), "%d", ipos );

        BOOST_THROW_EXCEPTION( std::runtime_error( std::string( "Invalid UUID string at position " ) + buffer + ": " + error ) );
    }

    BOOST_UUID_CXX14_CONSTEXPR unsigned char get_value( char c, int ipos ) const
    {
        constexpr char digits[] = "0123456789abcdefABCDEF";
        constexpr std::size_t digits_len = sizeof(digits) / sizeof(char) - 1;

        constexpr unsigned char values[] =
            { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,10,11,12,13,14,15 };

        auto pos = detail::cx_find( digits, digits_len, c );

        if( pos == 0 )
        {
            throw_invalid( ipos, "hex digit expected" );
        }

        return values[ pos - digits ];
    }

    BOOST_UUID_CXX14_CONSTEXPR unsigned char get_value( wchar_t c, int ipos ) const
    {
        constexpr wchar_t digits[] = L"0123456789abcdefABCDEF";
        constexpr std::size_t digits_len = sizeof(digits) / sizeof(wchar_t) - 1;

        constexpr unsigned char values[] =
            { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,10,11,12,13,14,15 };

        auto pos = detail::cx_find( digits, digits_len, c );

        if( pos == 0 )
        {
            throw_invalid( ipos, "hex digit expected" );
        }

        return values[ pos - digits ];
    }

    static constexpr bool is_dash( char c )
    {
        return c == '-';
    }

    static constexpr bool is_dash( wchar_t c )
    {
        return c == L'-';
    }

    static constexpr bool is_open_brace( char c )
    {
        return c == '{';
    }

    static constexpr bool is_open_brace( wchar_t c )
    {
        return c == L'{';
    }

    static constexpr bool is_close_brace( char c )
    {
        return c == '}';
    }

    static constexpr bool is_close_brace( wchar_t c )
    {
        return c == L'}';
    }
};

}} // namespace boost::uuids

#undef BOOST_UUID_CXX14_CONSTEXPR

#endif // BOOST_UUID_STRING_GENERATOR_HPP_INCLUDED

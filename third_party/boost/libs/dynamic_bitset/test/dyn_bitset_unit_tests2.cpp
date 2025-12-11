// -----------------------------------------------------------
//              Copyright (c) 2001 Jeremy Siek
//         Copyright (c) 2003-2006, 2025 Gennaro Prota
//             Copyright (c) 2014 Ahmed Charles
//             Copyright (c) 2018 Evgeny Shulgin
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// -----------------------------------------------------------

#include "bitset_test.hpp"
#include "boost/dynamic_bitset/dynamic_bitset.hpp"

template< typename Block, typename AllocatorOrContainer = std::allocator< Block > >
void
run_test_cases()
{
    typedef boost::dynamic_bitset< Block, AllocatorOrContainer > bitset_type;
    typedef bitset_test< bitset_type >     Tests;
    const int                              bits_per_block = bitset_type::bits_per_block;

    const std::string                      long_string    = get_long_string();

    //=====================================================================
    // Test operator&=
    {
        bitset_type lhs, rhs;
        Tests::and_assignment( lhs, rhs );
    }
    {
        bitset_type lhs( std::string( "1" ) ), rhs( std::string( "0" ) );
        Tests::and_assignment( lhs, rhs );
    }
    {
        bitset_type lhs( long_string.size(), 0 ), rhs( long_string );
        Tests::and_assignment( lhs, rhs );
    }
    {
        bitset_type lhs( long_string.size(), 1 ), rhs( long_string );
        Tests::and_assignment( lhs, rhs );
    }
    //=====================================================================
    // Test operator |=
    {
        bitset_type lhs, rhs;
        Tests::or_assignment( lhs, rhs );
    }
    {
        bitset_type lhs( std::string( "1" ) ), rhs( std::string( "0" ) );
        Tests::or_assignment( lhs, rhs );
    }
    {
        bitset_type lhs( long_string.size(), 0 ), rhs( long_string );
        Tests::or_assignment( lhs, rhs );
    }
    {
        bitset_type lhs( long_string.size(), 1 ), rhs( long_string );
        Tests::or_assignment( lhs, rhs );
    }
    //=====================================================================
    // Test operator^=
    {
        bitset_type lhs, rhs;
        Tests::xor_assignment( lhs, rhs );
    }
    {
        bitset_type lhs( std::string( "1" ) ), rhs( std::string( "0" ) );
        Tests::xor_assignment( lhs, rhs );
    }
    {
        bitset_type lhs( std::string( "0" ) ), rhs( std::string( "1" ) );
        Tests::xor_assignment( lhs, rhs );
    }
    {
        bitset_type lhs( long_string ), rhs( long_string );
        Tests::xor_assignment( lhs, rhs );
    }
    //=====================================================================
    // Test operator-=
    {
        bitset_type lhs, rhs;
        Tests::sub_assignment( lhs, rhs );
    }
    {
        bitset_type lhs( std::string( "1" ) ), rhs( std::string( "0" ) );
        Tests::sub_assignment( lhs, rhs );
    }
    {
        bitset_type lhs( std::string( "0" ) ), rhs( std::string( "1" ) );
        Tests::sub_assignment( lhs, rhs );
    }
    {
        bitset_type lhs( long_string ), rhs( long_string );
        Tests::sub_assignment( lhs, rhs );
    }
    //=====================================================================
    // Test operator<<=
    { // case pos == 0
        std::size_t pos = 0;
        {
            bitset_type b;
            Tests::shift_left_assignment( b, pos );
        }
        {
            bitset_type b( std::string( "1010" ) );
            Tests::shift_left_assignment( b, pos );
        }
        {
            bitset_type b( long_string );
            Tests::shift_left_assignment( b, pos );
        }
    }
    {
        // test with both multiple and
        // non multiple of bits_per_block
        const int how_many = 10;
        for ( int i = 1; i <= how_many; ++i ) {
            std::size_t                    multiple     = i * bits_per_block;
            std::size_t                    non_multiple = multiple - 1;
            bitset_type b( long_string );

            Tests::shift_left_assignment( b, multiple );
            Tests::shift_left_assignment( b, non_multiple );
        }
    }
    { // case pos == size()/2
        std::size_t                    pos = long_string.size() / 2;
        bitset_type b( long_string );
        Tests::shift_left_assignment( b, pos );
    }
    { // case pos >= n
        std::size_t                    pos = long_string.size();
        bitset_type b( long_string );
        Tests::shift_left_assignment( b, pos );
    }
    //=====================================================================
    // Test operator>>=
    { // case pos == 0
        std::size_t pos = 0;
        {
            bitset_type b;
            Tests::shift_right_assignment( b, pos );
        }
        {
            bitset_type b( std::string( "1010" ) );
            Tests::shift_right_assignment( b, pos );
        }
        {
            bitset_type b( long_string );
            Tests::shift_right_assignment( b, pos );
        }
    }
    {
        // test with both multiple and
        // non multiple of bits_per_block
        const int how_many = 10;
        for ( int i = 1; i <= how_many; ++i ) {
            std::size_t                    multiple     = i * bits_per_block;
            std::size_t                    non_multiple = multiple - 1;
            bitset_type b( long_string );

            Tests::shift_right_assignment( b, multiple );
            Tests::shift_right_assignment( b, non_multiple );
        }
    }
    { // case pos == size()/2
        std::size_t                    pos = long_string.size() / 2;
        bitset_type b( long_string );
        Tests::shift_right_assignment( b, pos );
    }
    { // case pos >= n
        std::size_t                    pos = long_string.size();
        bitset_type b( long_string );
        Tests::shift_right_assignment( b, pos );
    }
    //=====================================================================
    // test b.set()
    {
        bitset_type b;
        Tests::set_all( b );
    }
    {
        bitset_type b( std::string( "0" ) );
        Tests::set_all( b );
    }
    {
        bitset_type b( long_string );
        Tests::set_all( b );
    }
    //=====================================================================
    // Test b.set(pos)
    { // case pos >= b.size()
        bitset_type b;
        Tests::set_one( b, 0, true );
        Tests::set_one( b, 0, false );
    }
    { // case pos < b.size()
        bitset_type b( std::string( "0" ) );
        Tests::set_one( b, 0, true );
        Tests::set_one( b, 0, false );
    }
    { // case pos == b.size() / 2
        bitset_type b( long_string );
        Tests::set_one( b, long_string.size() / 2, true );
        Tests::set_one( b, long_string.size() / 2, false );
    }
    //=====================================================================
    // Test b.set(pos, len)

      // case size is 0
    {
        bitset_type b( std::string( "10" ) ) ;
        Tests::set_segment( b, 0, 0, true );
        Tests::set_segment( b, 0, 0, false );
    }
    { // case size is 1
        bitset_type b( std::string( "0" ) );
        Tests::set_segment( b, 0, 1, true );
        Tests::set_segment( b, 0, 1, false );
    }
    { // case fill the whole set
        bitset_type b( long_string );
        Tests::set_segment( b, 0, b.size(), true );
        Tests::set_segment( b, 0, b.size(), false );
    }
    { // case pos = size / 4, len = size / 2
        bitset_type b( long_string );
        Tests::set_segment( b, b.size() / 4, b.size() / 2, true );
        Tests::set_segment( b, b.size() / 4, b.size() / 2, false );
    }
    { // case pos = block_size / 2, len = size - block_size
        bitset_type b( long_string );
        Tests::set_segment( b, bitset_type::bits_per_block / 2, b.size() - bitset_type::bits_per_block, true );
        Tests::set_segment( b, bitset_type::bits_per_block / 2, b.size() - bitset_type::bits_per_block, false );
    }
    { // case pos = 1, len = size - 2
        bitset_type b( long_string );
        Tests::set_segment( b, 1, b.size() - 2, true );
        Tests::set_segment( b, 1, b.size() - 2, false );
    }
    { // case pos = 3, len = 7
        bitset_type b( long_string );
        Tests::set_segment( b, 3, 7, true );
        Tests::set_segment( b, 3, 7, false );
    }
    //=====================================================================
    // Test b.reset()
    {
        bitset_type b;
        Tests::reset_all( b );
    }
    {
        bitset_type b( std::string( "0" ) );
        Tests::reset_all( b );
    }
    {
        bitset_type b( long_string );
        Tests::reset_all( b );
    }
    //=====================================================================
    // Test b.reset(pos)
    { // case pos >= b.size()
        bitset_type b;
        Tests::reset_one( b, 0 );
    }
    { // case pos < b.size()
        bitset_type b( std::string( "0" ) );
        Tests::reset_one( b, 0 );
    }
    { // case pos == b.size() / 2
        bitset_type b( long_string );
        Tests::reset_one( b, long_string.size() / 2 );
    }
    //=====================================================================
    // Test b.reset(pos, len)
    { // case size is 1
        bitset_type b( std::string( "0" ) );
        Tests::reset_segment( b, 0, 1 );
    }
    { // case fill the whole set
        bitset_type b( long_string );
        Tests::reset_segment( b, 0, b.size() );
    }
    { // case pos = size / 4, len = size / 2
        bitset_type b( long_string );
        Tests::reset_segment( b, b.size() / 4, b.size() / 2 );
    }
    { // case pos = block_size / 2, len = size - block_size
        bitset_type b( long_string );
        Tests::reset_segment( b, bitset_type::bits_per_block / 2, b.size() - bitset_type::bits_per_block );
    }
    { // case pos = 1, len = size - 2
        bitset_type b( long_string );
        Tests::reset_segment( b, 1, b.size() - 2 );
    }
    { // case pos = 3, len = 7
        bitset_type b( long_string );
        Tests::reset_segment( b, 3, 7 );
    }
    //=====================================================================
    // Test ~b
    {
        bitset_type b;
        Tests::operator_flip( b );
    }
    {
        bitset_type b( std::string( "1" ) );
        Tests::operator_flip( b );
    }
    {
        bitset_type b( long_string );
        Tests::operator_flip( b );
    }
    //=====================================================================
    // Test b.flip()
    {
        bitset_type b;
        Tests::flip_all( b );
    }
    {
        bitset_type b( std::string( "1" ) );
        Tests::flip_all( b );
    }
    {
        bitset_type b( long_string );
        Tests::flip_all( b );
    }
    //=====================================================================
    // Test b.flip(pos)
    { // case pos >= b.size()
        bitset_type b;
        Tests::flip_one( b, 0 );
    }
    { // case pos < b.size()
        bitset_type b( std::string( "0" ) );
        Tests::flip_one( b, 0 );
    }
    { // case pos == b.size() / 2
        bitset_type b( long_string );
        Tests::flip_one( b, long_string.size() / 2 );
    }
    //=====================================================================
    // Test b.flip(pos, len)
    { // case size is 1
        bitset_type b( std::string( "0" ) );
        Tests::flip_segment( b, 0, 1 );
    }
    { // case fill the whole set
        bitset_type b( long_string );
        Tests::flip_segment( b, 0, b.size() );
    }
    { // case pos = size / 4, len = size / 2
        bitset_type b( long_string );
        Tests::flip_segment( b, b.size() / 4, b.size() / 2 );
    }
    { // case pos = block_size / 2, len = size - block_size
        bitset_type b( long_string );
        Tests::flip_segment( b, bitset_type::bits_per_block / 2, b.size() - bitset_type::bits_per_block );
    }
    { // case pos = 1, len = size - 2
        bitset_type b( long_string );
        Tests::flip_segment( b, 1, b.size() - 2 );
    }
    { // case pos = 3, len = 7
        bitset_type b( long_string );
        Tests::flip_segment( b, 3, 7 );
    }
}

int
main()
{
    run_test_cases< unsigned char >();
    run_test_cases< unsigned char, small_vector< unsigned char > >();
    run_test_cases< unsigned short >();
    run_test_cases< unsigned short, small_vector< unsigned short > >();
    run_test_cases< unsigned int >();
    run_test_cases< unsigned int, small_vector< unsigned int > >();
    run_test_cases< unsigned long >();
    run_test_cases< unsigned long, small_vector< unsigned long > >();
    run_test_cases< unsigned long long >();
    run_test_cases< unsigned long long, small_vector< unsigned long long > >();

    return boost::report_errors();
}

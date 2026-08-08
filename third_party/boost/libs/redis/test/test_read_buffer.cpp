/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/detail/read_buffer.hpp>
#include <boost/redis/error.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/system/error_code.hpp>

using namespace boost::redis;
using detail::read_buffer;
using boost::system::error_code;

// NOTE1: The vector allocation behavior depends on the implementation.
// The test might not pass if we start testing on a new platform.

namespace {

void test_prepare_equals_max()
{
   read_buffer buf{{10}};

   // Corner case: prepare equals max.
   auto const res = buf.prepare(10);
   BOOST_TEST_EQ(res.ec, error_code());
}

// TODO: bigger than max can happen in two situations. Check both
void test_prepare_bigger_than_max()
{
   read_buffer buf{{10}};
   auto const res = buf.prepare(11);
   BOOST_TEST_EQ(res.ec, error_code{error::exceeds_maximum_read_buffer_size});
}

/*    |      max size      |
 *
 * 1. |++++++++++++++++| prepare(16)
 * 2. |----------------| commit(16)
 * 3. |=========-------| consume(9)
 *
 * In this state the buffer has size 16 and a maximum configure size 20.
 * Preparing for another 5 bytes exceeds the max size by one but should not
 * fail since the 9 bytes in the front should be rotated by the implementation.
 *
 * 4. |=========-------| consume(9)
 * 5. |-------+++++| prepare(5)
 *
 */
void test_consume_avoids_prepare_max_error()
{
   read_buffer buf{{20}};

   auto res = buf.prepare(16);
   BOOST_TEST_EQ(res.ec, error_code());
   buf.commit(16);
   auto consumed = buf.consume(9);
   BOOST_TEST_EQ(consumed, 9u);

   res = buf.prepare(5);
   BOOST_TEST_EQ(res.ec, error_code());
   BOOST_TEST_EQ(res.rotated, 7u);
}

void test_prepare_consume_only_committed_data()
{
   read_buffer buf{{10}};

   auto res = buf.prepare(10);
   BOOST_TEST(!res.ec);
   BOOST_TEST_EQ(res.rotated, 0u);

   // No data has been committed yet so nothing can be consumed.
   auto consumed = buf.consume(5);
   BOOST_TEST_EQ(consumed, 0u);

   buf.commit(10);
   consumed = buf.consume(5);

   // All five bytes should have been consumed.
   BOOST_TEST_EQ(consumed, 5u);

   consumed = buf.consume(7);

   // Only the remaining five bytes can be consumed
   BOOST_TEST_EQ(consumed, 5u);
}

void test_check_buffer_size()
{
   read_buffer buf{{10}};

   auto res = buf.prepare(10);
   BOOST_TEST_EQ(res.ec, error_code());
   BOOST_TEST_EQ(res.rotated, 0u);

   BOOST_TEST_EQ(buf.get_prepared().size(), 10u);
}

void test_prepared_erased_after_commit()
{
   read_buffer buf;

   auto res = buf.prepare(10);
   BOOST_TEST_EQ(res.ec, error_code());
   BOOST_TEST_EQ(res.rotated, 0u);

   buf.commit(7);
   auto prep = buf.get_prepared().size();
   BOOST_TEST_EQ(prep, 0u);

   res = buf.prepare(10);
   prep = buf.get_prepared().size();
   BOOST_TEST_EQ(prep, 10u);
}

/* 1. |++++++++++|         - prepare(10)
 * 2. |-------|            - commit(7)
 * 3. |-------++++++++++|  - prepare(10)
 * 4. |--------------|     - commit(7)
 * 5. |======--------|     - consume(5)
 */
void test_prep_commit_consume_sizes()
{
   read_buffer buf;

   // 1.
   auto res = buf.prepare(10);
   BOOST_TEST_EQ(res.ec, error_code());
   BOOST_TEST_EQ(res.rotated, 0u);

   // 2.
   buf.commit(7);
   BOOST_TEST_EQ(buf.size(), 7u);
   auto prep = buf.get_prepared().size();
   BOOST_TEST_EQ(prep, 0u);
   BOOST_TEST_EQ(buf.get_commited().size(), 7u);

   // 3.
   res = buf.prepare(10);
   BOOST_TEST_EQ(buf.size(), 17u);
   prep = buf.get_prepared().size();
   BOOST_TEST_EQ(prep, 10u);
 
   // 4.
   buf.commit(7);
   BOOST_TEST_EQ(buf.size(), 14u);
   prep = buf.get_prepared().size();
   BOOST_TEST_EQ(prep, 0u);
   BOOST_TEST_EQ(buf.get_commited().size(), 14u);

   // 5.
   buf.consume(5);
   BOOST_TEST_EQ(buf.size(), 14u);
   prep = buf.get_prepared().size();
   BOOST_TEST_EQ(prep, 0u);
   BOOST_TEST_EQ(buf.get_commited().size(), 9u);
}

/*    |          capacity       |
 *
 * 1. |++++++++++++++++++++| prepare(20)
 * 2. |--------------------| commit(20)
 * 3. |===============-----| consume(15)
 * 4. |-----++++++|          prepare(6)
 *
 * Without rotation that last step would cause reallocation. The implementation
 * should avoid that.
 */
void test_prepare_rotates_to_avoid_realloc()
{
   read_buffer buf;
   buf.reserve(25);
   BOOST_TEST_EQ(buf.capacity(), 25);

   // 1.
   auto res = buf.prepare(20);
   BOOST_TEST_EQ(res.ec, error_code());

   // 2.
   buf.commit(20);

   // 3.
   auto consumed = buf.consume(15);
   BOOST_TEST_EQ(consumed, 15u);

   // 4.
   res = buf.prepare(6);
   BOOST_TEST_EQ(res.ec, error_code());
   BOOST_TEST_EQ(res.rotated, 5u);
   BOOST_TEST_EQ(buf.capacity(), 25);
}

/*  1. |++++++++++++++++|     prepare(16)
 *  2. |----------|           commit(10)
 *  3. |----------++++++++++| prepare(10)
 *
 *  Step 3. should rotate no data since there is no consumed data.
 */
void test_no_rotation_when_consumed_zero()
{
   read_buffer buf;
   buf.reserve(20);
   BOOST_TEST_EQ(buf.capacity(), 20);

   // 1.
   auto res = buf.prepare(16);
   BOOST_TEST_EQ(res.ec, error_code());
   BOOST_TEST_EQ(res.rotated, 0u);

   // 2.
   buf.commit(10);

   // 3.
   res = buf.prepare(10);
   BOOST_TEST_EQ(res.ec, error_code());
   BOOST_TEST_EQ(res.rotated, 0u);
}

}  // namespace

int main()
{
   test_prepare_consume_only_committed_data();
   test_check_buffer_size();
   test_prepared_erased_after_commit();
   test_prep_commit_consume_sizes();
   test_prepare_equals_max();
   test_prepare_bigger_than_max();
   test_consume_avoids_prepare_max_error();
   test_prepare_rotates_to_avoid_realloc();
   test_no_rotation_when_consumed_zero();

   return boost::report_errors();
}

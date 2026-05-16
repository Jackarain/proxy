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

namespace {

void test_prepare_error()
{
   read_buffer buf;

   // Usual case, max size is bigger then requested size.
   buf.set_config({10, 10});
   auto ec = buf.prepare();
   BOOST_TEST_EQ(ec, error_code());
   buf.commit(10);

   // Corner case, max size is equal to the requested size.
   buf.set_config({10, 20});
   ec = buf.prepare();
   BOOST_TEST_EQ(ec, error_code());
   buf.commit(10);
   buf.consume(20);

   auto const tmp = buf;

   // Error case, max size is smaller to the requested size.
   buf.set_config({10, 9});
   ec = buf.prepare();
   BOOST_TEST_EQ(ec, error_code{error::exceeds_maximum_read_buffer_size});

   // Check that an error call has no side effects.
   BOOST_TEST(buf == tmp);
}

void test_prepare_consume_only_committed_data()
{
   read_buffer buf;

   buf.set_config({10, 10});
   auto ec = buf.prepare();
   BOOST_TEST(!ec);

   auto res = buf.consume(5);

   // No data has been committed yet so nothing can be consummed.
   BOOST_TEST_EQ(res.consumed, 0u);

   // If nothing was consumed, nothing got rotated.
   BOOST_TEST_EQ(res.rotated, 0u);

   buf.commit(10);
   res = buf.consume(5);

   // All five bytes should have been consumed.
   BOOST_TEST_EQ(res.consumed, 5u);

   // We added a total of 10 bytes and consumed 5, that means, 5 were
   // rotated.
   BOOST_TEST_EQ(res.rotated, 5u);

   res = buf.consume(7);

   // Only the remaining five bytes can be consumed
   BOOST_TEST_EQ(res.consumed, 5u);

   // No bytes to rotated.
   BOOST_TEST_EQ(res.rotated, 0u);
}

void test_check_buffer_size()
{
   read_buffer buf;

   buf.set_config({10, 10});
   auto ec = buf.prepare();
   BOOST_TEST_EQ(ec, error_code());

   BOOST_TEST_EQ(buf.get_prepared().size(), 10u);
}

}  // namespace

int main()
{
   test_prepare_error();
   test_prepare_consume_only_committed_data();
   test_check_buffer_size();

   return boost::report_errors();
}

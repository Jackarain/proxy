//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/impl/log_to_file.hpp>

#include <boost/core/lightweight_test.hpp>

#include <cstddef>
#include <cstdio>
#include <limits>
#include <memory>
#include <string>
#include <string_view>

using namespace boost::redis;

namespace {

// RAII helpers for working with C FILE*
struct file_deleter {
   void operator()(FILE* f) const { std::fclose(f); }
};
using unique_file = std::unique_ptr<FILE, file_deleter>;

unique_file create_temporary()
{
   unique_file f{std::tmpfile()};
   if (!BOOST_TEST_NE(f.get(), nullptr))
      exit(1);
   return f;
}

std::string get_file_contents(FILE* f)
{
   if (!BOOST_TEST_EQ(std::fseek(f, 0, SEEK_END), 0))
      exit(1);
   long fsize = std::ftell(f);
   if (!BOOST_TEST_GE(fsize, 0))
      exit(1);
   std::rewind(f);
   std::string res(fsize, 0);
   if (!BOOST_TEST_EQ(std::fread(res.data(), 1u, res.size(), f), fsize))
      exit(1);
   return res;
}

void test_regular()
{
   auto f = create_temporary();
   detail::log_to_file(f.get(), "something happened");
   BOOST_TEST_EQ(get_file_contents(f.get()), "(Boost.Redis) something happened\n");
}

void test_empty_message()
{
   auto f = create_temporary();
   detail::log_to_file(f.get(), {});
   BOOST_TEST_EQ(get_file_contents(f.get()), "(Boost.Redis) \n");
}

void test_empty_prefix()
{
   auto f = create_temporary();
   detail::log_to_file(f.get(), {}, "");
   BOOST_TEST_EQ(get_file_contents(f.get()), "\n");
}

void test_message_not_null_terminated()
{
   constexpr std::string_view str = "some_string";
   auto f = create_temporary();
   detail::log_to_file(f.get(), str.substr(0, 4));
   BOOST_TEST_EQ(get_file_contents(f.get()), "(Boost.Redis) some\n");
}

// NULL bytes don't cause UB. None of our messages have
// them, so this is an edge case
void test_message_null_bytes()
{
   char buff[] = {'a', 'b', 'c', 0, 'l', 0};
   auto f = create_temporary();
   detail::log_to_file(f.get(), std::string_view(buff, sizeof(buff)));
   BOOST_TEST_EQ(get_file_contents(f.get()), "(Boost.Redis) abc\n");
}

// Internally, sizes are converted to int because of C APIs. Check that this
// does not cause trouble. We impose a sanity limit of 0xffff bytes for all messages
void test_message_very_long()
{
   // Setup. Allocating a string of size INT_MAX causes trouble, so we pass a string_view
   // with that size, but with only the first 0xffff bytes being valid
   std::string msg(0xffffu + 1u, 'a');
   const auto msg_size = static_cast<std::size_t>((std::numeric_limits<int>::max)()) + 1u;
   auto f = create_temporary();

   // Log
   detail::log_to_file(f.get(), std::string_view(msg.data(), msg_size));

   // Check
   std::string expected = "(Boost.Redis) ";
   expected += std::string_view(msg.data(), 0xffffu);
   expected += '\n';
   BOOST_TEST_EQ(get_file_contents(f.get()), expected);
}

}  // namespace

int main()
{
   test_regular();
   test_empty_message();
   test_empty_prefix();
   test_message_not_null_terminated();
   test_message_null_bytes();
   test_message_very_long();

   return boost::report_errors();
}

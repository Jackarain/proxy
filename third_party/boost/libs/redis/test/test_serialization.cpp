//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/request.hpp>
#include <boost/redis/resp3/serialization.hpp>

#include <boost/core/lightweight_test.hpp>

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using boost::redis::request;

namespace other {

struct my_struct {
   int value;
};

void boost_redis_to_bulk(std::string& to, my_struct value)
{
   boost::redis::resp3::boost_redis_to_bulk(to, value.value);
}

}  // namespace other

namespace {

// --- Strings ---
void test_string_view()
{
   request req;
   req.push("GET", std::string_view("key"));
   BOOST_TEST_EQ(req.payload(), "*2\r\n$3\r\nGET\r\n$3\r\nkey\r\n");
}

void test_string()
{
   std::string s{"k1"};
   const std::string s2{"k2"};
   request req;
   req.push("GET", s, s2, std::string("k3"));
   BOOST_TEST_EQ(req.payload(), "*4\r\n$3\r\nGET\r\n$2\r\nk1\r\n$2\r\nk2\r\n$2\r\nk3\r\n");
}

void test_c_string()
{
   request req;
   req.push("GET", "k1", static_cast<const char*>("k2"));
   BOOST_TEST_EQ(req.payload(), "*3\r\n$3\r\nGET\r\n$2\r\nk1\r\n$2\r\nk2\r\n");
}

void test_string_empty()
{
   request req;
   req.push("GET", std::string_view{});
   BOOST_TEST_EQ(req.payload(), "*2\r\n$3\r\nGET\r\n$0\r\n\r\n");
}

// --- Integers ---
void test_signed_ints()
{
   request req;
   req.push("GET", static_cast<signed char>(20), static_cast<short>(-42), -1, 80l, 200ll);
   BOOST_TEST_EQ(
      req.payload(),
      "*6\r\n$3\r\nGET\r\n$2\r\n20\r\n$3\r\n-42\r\n$2\r\n-1\r\n$2\r\n80\r\n$3\r\n200\r\n");
}

void test_unsigned_ints()
{
   request req;
   req.push(
      "GET",
      static_cast<unsigned char>(20),
      static_cast<unsigned short>(42),
      50u,
      80ul,
      200ull);
   BOOST_TEST_EQ(
      req.payload(),
      "*6\r\n$3\r\nGET\r\n$2\r\n20\r\n$2\r\n42\r\n$2\r\n50\r\n$2\r\n80\r\n$3\r\n200\r\n");
}

// We don't overflow for big ints
void test_signed_ints_minmax()
{
   using lims = std::numeric_limits<std::int64_t>;
   request req;
   req.push("GET", (lims::min)(), (lims::max)());
   BOOST_TEST_EQ(
      req.payload(),
      "*3\r\n$3\r\nGET\r\n$20\r\n-9223372036854775808\r\n$19\r\n9223372036854775807\r\n");
}

void test_unsigned_ints_max()
{
   request req;
   req.push("GET", (std::numeric_limits<std::uint64_t>::max)());
   BOOST_TEST_EQ(req.payload(), "*2\r\n$3\r\nGET\r\n$20\r\n18446744073709551615\r\n");
}

// Custom type
void test_custom()
{
   request req;
   req.push("GET", other::my_struct{42});
   BOOST_TEST_EQ(req.payload(), "*2\r\n$3\r\nGET\r\n$2\r\n42\r\n");
}

// --- Pairs and tuples (only supported in the range versions) ---
// Nested structures are not supported (compile time error)
void test_pair()
{
   std::vector<std::pair<std::string_view, int>> vec{
      {"k1", 42}
   };
   request req;
   req.push_range("GET", vec);
   BOOST_TEST_EQ(req.payload(), "*3\r\n$3\r\nGET\r\n$2\r\nk1\r\n$2\r\n42\r\n");
}

void test_pair_custom()
{
   std::vector<std::pair<std::string_view, other::my_struct>> vec{
      {"k1", {42}}
   };
   request req;
   req.push_range("GET", vec);
   BOOST_TEST_EQ(req.payload(), "*3\r\n$3\r\nGET\r\n$2\r\nk1\r\n$2\r\n42\r\n");
}

void test_tuple()
{
   std::vector<std::tuple<std::string_view, int, unsigned char>> vec{
      {"k1", 42, 1}
   };
   request req;
   req.push_range("GET", vec);
   BOOST_TEST_EQ(req.payload(), "*4\r\n$3\r\nGET\r\n$2\r\nk1\r\n$2\r\n42\r\n$1\r\n1\r\n");
}

void test_tuple_custom()
{
   std::vector<std::tuple<std::string_view, other::my_struct>> vec{
      {"k1", {42}}
   };
   request req;
   req.push_range("GET", vec);
   BOOST_TEST_EQ(req.payload(), "*3\r\n$3\r\nGET\r\n$2\r\nk1\r\n$2\r\n42\r\n");
}

void test_tuple_empty()
{
   std::vector<std::tuple<>> vec{{}};
   request req;
   req.push_range("GET", vec);
   BOOST_TEST_EQ(req.payload(), "*1\r\n$3\r\nGET\r\n");
}

}  // namespace

int main()
{
   test_string_view();
   test_string();
   test_c_string();
   test_string_empty();

   test_signed_ints();
   test_unsigned_ints();
   test_signed_ints_minmax();
   test_unsigned_ints_max();

   test_custom();

   test_pair();
   test_pair_custom();
   test_tuple();
   test_tuple_custom();
   test_tuple_empty();

   return boost::report_errors();
}

//
// Copyright (c) 2026 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/push_parser.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/system/error_code.hpp>

#include "sansio_utils.hpp"

#include <iterator>
#include <optional>
#include <vector>
#if (__cpp_lib_ranges >= 201911L) && (__cpp_lib_concepts >= 202002L)
#define BOOST_REDIS_TEST_RANGE_CONCEPTS
#include <ranges>
#endif

using namespace boost::redis;
using detail::tree_from_resp3;

// Operators
namespace boost::redis {

std::ostream& operator<<(std::ostream& os, const push_view& v)
{
   os << "push_view { .channel=" << v.channel;
   if (v.pattern)
      os << ", .pattern=" << *v.pattern;
   return os << ", .payload=" << v.payload << " }";
}

bool operator==(const push_view& lhs, const push_view& rhs) noexcept
{
   return lhs.channel == rhs.channel && lhs.pattern == rhs.pattern && lhs.payload == rhs.payload;
}

}  // namespace boost::redis

namespace {

// --- Only valid messages ---
void test_one_message()
{
   auto nodes = tree_from_resp3({">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n"});
   push_parser p{nodes};

   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

void test_several_messages()
{
   auto nodes = tree_from_resp3({
      ">3\r\n$7\r\nmessage\r\n$5\r\nfirst\r\n$2\r\nHi\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$3\r\nBye\r\n",
   });
   push_parser p{nodes};

   const push_view expected[] = {
      {"first",  std::nullopt, "Hi" },
      {"second", std::nullopt, "Bye"},
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

void test_one_pmessage()
{
   auto nodes = tree_from_resp3(
      {">4\r\n$8\r\npmessage\r\n$6\r\nmycha*\r\n$6\r\nmychan\r\n$5\r\nHello\r\n"});
   push_parser p{nodes};

   const push_view expected[] = {
      {"mychan", "mycha*", "Hello"},
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

void test_mixed_valid()
{
   auto nodes = tree_from_resp3({
      ">3\r\n$7\r\nmessage\r\n$3\r\nch1\r\n$4\r\nmsg1\r\n",
      ">4\r\n$8\r\npmessage\r\n$1\r\n*\r\n$3\r\nch2\r\n$4\r\nmsg2\r\n",
      ">3\r\n$7\r\nmessage\r\n$3\r\nch3\r\n$4\r\nmsg3\r\n",
   });
   push_parser p{nodes};

   const push_view expected[] = {
      {"ch1", std::nullopt, "msg1"},
      {"ch2", "*",          "msg2"},
      {"ch3", std::nullopt, "msg3"},
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

void test_message_empty_fields()
{
   auto nodes = tree_from_resp3({">3\r\n$7\r\nmessage\r\n$0\r\n\r\n$0\r\n\r\n"});
   push_parser p{nodes};

   const push_view expected[] = {
      {"", std::nullopt, ""},
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

void test_pmessage_empty_fields()
{
   auto nodes = tree_from_resp3({">4\r\n$8\r\npmessage\r\n$0\r\n\r\n$0\r\n\r\n$0\r\n\r\n"});
   push_parser p{nodes};

   const push_view expected[] = {
      {"", "", ""},
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

// --- Message skipping (valid, expected messages we don't care about) ---

// Anything that is not a RESP3 push is skipped.
// Concretely, pushes in RESP2 used to be arrays. We don't support these
void test_skip_type_array()
{
   auto nodes = tree_from_resp3({
      "*3\r\n$7\r\nmessage\r\n$5\r\nfirst\r\n$5\r\nValue\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
   });
   push_parser p{nodes};

   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

// Might happen if a SUBSCRIBE call fails
void test_skip_type_simple_error()
{
   auto nodes = tree_from_resp3({
      "-ERR foo\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
   });
   push_parser p{nodes};

   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

// Might happen when using MONITOR
void test_skip_type_simple_string()
{
   auto nodes = tree_from_resp3({
      "+MONITOR output\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
   });
   push_parser p{nodes};

   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

// Edge case: other RESP3 types are skipped
void test_skip_type_other()
{
   auto nodes = tree_from_resp3({
      "%1\r\n$3\r\nkey\r\n$5\r\nvalue\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
   });
   push_parser p{nodes};

   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

// The push first field is not 'message' or 'pmessage' (e.g. subscribe confirmations)
void test_skip_msgtype_subscribe_confirmations()
{
   auto nodes = tree_from_resp3({
      ">3\r\n$9\r\nsubscribe\r\n$6\r\nmychan\r\n:1\r\n",
      ">3\r\n$11\r\nunsubscribe\r\n$6\r\nmychan\r\n:1\r\n",
      ">3\r\n$10\r\npsubscribe\r\n$5\r\np*tt*\r\n:1\r\n",
      ">3\r\n$12\r\npunsubscribe\r\n$5\r\np*tt*\r\n:1\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
   });
   push_parser p{nodes};

   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

// Message type string, but with an unknown value (not 'message' or 'pmessage')
void test_skip_msgtype_unknown()
{
   auto nodes = tree_from_resp3({
      ">3\r\n$7\r\nunknown\r\n$4\r\nchan\r\n$4\r\nbody\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
   });
   push_parser p{nodes};

   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

// Message type is an empty string
void test_skip_msgtype_empty()
{
   auto nodes = tree_from_resp3({
      ">3\r\n$0\r\n\r\n$4\r\nchan\r\n$4\r\nbody\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
   });
   push_parser p{nodes};

   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

// Message type is not a string (e.g. array)
void test_skip_msgtype_not_string()
{
   auto nodes = tree_from_resp3({
      ">3\r\n*1\r\n$3\r\nfoo\r\n$5\r\nHello\r\n$5\r\nworld\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
   });
   push_parser p{nodes};

   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

// The message ends before the message type
void test_skip_msgtype_eof()
{
   auto nodes = tree_from_resp3({
      ">0\r\n",  // end of message manifests as another message starting
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
      ">0\r\n",  // end of message manifests as the end of the range
   });
   push_parser p{nodes};

   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

// Pattern is not a string
void test_skip_pattern_not_string()
{
   auto nodes = tree_from_resp3({
      ">4\r\n$8\r\npmessage\r\n*1\r\n$3\r\nfoo\r\n$6\r\nmychan\r\n$5\r\nHello\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
   });
   push_parser p{nodes};
   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

// The message ends before the pattern field
void test_skip_pattern_eof()
{
   auto nodes = tree_from_resp3({
      ">1\r\n$8\r\npmessage\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
      ">1\r\n$8\r\npmessage\r\n",
   });
   push_parser p{nodes};
   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

// Channel not a string
void test_skip_channel_not_string_message()
{
   auto nodes = tree_from_resp3({
      ">3\r\n$7\r\nmessage\r\n*1\r\n$3\r\nfoo\r\n$5\r\nHello\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
   });
   push_parser p{nodes};
   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

void test_skip_channel_not_string_pmessage()
{
   auto nodes = tree_from_resp3({
      ">4\r\n$8\r\npmessage\r\n$1\r\n*\r\n*1\r\n$3\r\nfoo\r\n$5\r\nHello\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
   });
   push_parser p{nodes};
   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

// Message ends before channel
void test_skip_channel_eof_message()
{
   auto nodes = tree_from_resp3({
      ">1\r\n$7\r\nmessage\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
      ">1\r\n$7\r\nmessage\r\n",
   });
   push_parser p{nodes};
   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

void test_skip_channel_eof_pmessage()
{
   auto nodes = tree_from_resp3({
      ">2\r\n$8\r\npmessage\r\n$1\r\n*\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
      ">2\r\n$8\r\npmessage\r\n$1\r\n*\r\n",
   });
   push_parser p{nodes};
   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

// Payload not a string
void test_skip_payload_not_string_message()
{
   auto nodes = tree_from_resp3({
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n*1\r\n$3\r\nfoo\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
   });
   push_parser p{nodes};
   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

void test_skip_payload_not_string_pmessage()
{
   auto nodes = tree_from_resp3({
      ">4\r\n$8\r\npmessage\r\n$1\r\n*\r\n$6\r\nsecond\r\n*1\r\n$3\r\nfoo\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
   });
   push_parser p{nodes};
   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

// Message ends before payload
void test_skip_payload_eof_pmessage()
{
   auto nodes = tree_from_resp3({
      ">3\r\n$8\r\npmessage\r\n$1\r\n*\r\n$3\r\nch2\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
      ">3\r\n$8\r\npmessage\r\n$1\r\n*\r\n$3\r\nch2\r\n",
   });
   push_parser p{nodes};
   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

void test_skip_payload_eof_message()
{
   auto nodes = tree_from_resp3({
      ">2\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
      ">2\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n",
   });
   push_parser p{nodes};
   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

// Message is longer than expected
void test_skip_longer_message()
{
   auto nodes = tree_from_resp3({
      ">4\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n$1\r\nx\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
   });
   push_parser p{nodes};
   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

void test_skip_longer_pmessage()
{
   auto nodes = tree_from_resp3({
      ">5\r\n$8\r\npmessage\r\n$1\r\n*\r\n$3\r\nch2\r\n$4\r\nmsg2\r\n$1\r\nx\r\n",
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
   });
   push_parser p{nodes};
   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

// --- Mixes of valid and skipped messages ---
void test_only_skipped()
{
   auto nodes = tree_from_resp3({
      "+foo\r\n",
   });
   push_parser p{nodes};
   std::vector<push_view> expected;
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), expected.begin(), expected.end());
}

void test_valid_skipped()
{
   auto nodes = tree_from_resp3({
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
      "-ERR foo\r\n",
   });
   push_parser p{nodes};
   constexpr push_view expected[] = {
      {"second", std::nullopt, "Hello"}
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

void test_valid_skipped_mix()
{
   auto nodes = tree_from_resp3({
      ">3\r\n$7\r\nmessage\r\n$3\r\nch1\r\n$4\r\nmsg1\r\n",
      "+MONITOR\r\n",
      ">3\r\n$9\r\nsubscribe\r\n$4\r\nchan\r\n:1\r\n",
      ">3\r\n$7\r\nmessage\r\n$3\r\nch2\r\n$4\r\nmsg2\r\n",
      ">3\r\n$7\r\nmessage\r\n$3\r\nch3\r\n$4\r\nmsg3\r\n",
      "%1\r\n$3\r\nkey\r\n$5\r\nvalue\r\n",
   });
   push_parser p{nodes};
   const push_view expected[] = {
      {"ch1", std::nullopt, "msg1"},
      {"ch2", std::nullopt, "msg2"},
      {"ch3", std::nullopt, "msg3"},
   };
   BOOST_TEST_ALL_EQ(p.begin(), p.end(), std::begin(expected), std::end(expected));
}

// --- Edge cases ---
void test_empty()
{
   auto nodes = tree_from_resp3({});
   push_parser p{nodes};
   std::vector<push_view> expected;

   BOOST_TEST_ALL_EQ(p.begin(), p.end(), expected.begin(), expected.end());
}

// --- The range interface works as expected ---
#ifdef BOOST_REDIS_TEST_RANGE_CONCEPTS
static_assert(std::ranges::input_range<push_parser>);
static_assert(std::input_iterator<push_parser::iterator>);
#endif

void test_range_interface()
{
   auto nodes = tree_from_resp3({
      ">3\r\n$7\r\nmessage\r\n$6\r\nsecond\r\n$5\r\nHello\r\n",
   });

   // Iterator default construction
   {
      push_parser p{nodes};
      push_parser::iterator it;
      BOOST_TEST(it == p.end());
   }

   // Iterator comparison
   {
      push_parser p{nodes};
      BOOST_TEST(p.begin() == p.begin());
      BOOST_TEST(p.begin() != p.end());
      BOOST_TEST(p.end() == p.end());
   }

   // Iterator prefix increment
   {
      push_parser p{nodes};
      auto it = p.begin();
      ++it;
      BOOST_TEST(it == p.end());
   }

   // Iterator postfix increment
   {
      push_parser p{nodes};
      auto it = p.begin();
      it++;
      BOOST_TEST(it == p.end());
   }

   // Iterator dereference
   {
      push_parser p{nodes};
      auto it = p.begin();
      auto value = *it;
      BOOST_TEST_EQ(value.payload, "Hello");
   }

   // Iterator operator->
   {
      push_parser p{nodes};
      auto it = p.begin();
      BOOST_TEST_EQ(it->payload, "Hello");
   }
}

}  // namespace

int main()
{
   test_one_message();
   test_several_messages();
   test_one_pmessage();
   test_mixed_valid();
   test_message_empty_fields();
   test_pmessage_empty_fields();

   test_skip_type_array();
   test_skip_type_simple_error();
   test_skip_type_simple_string();
   test_skip_type_other();

   test_skip_msgtype_subscribe_confirmations();
   test_skip_msgtype_unknown();
   test_skip_msgtype_empty();
   test_skip_msgtype_not_string();
   test_skip_msgtype_eof();

   test_skip_pattern_not_string();
   test_skip_pattern_eof();

   test_skip_channel_not_string_message();
   test_skip_channel_not_string_pmessage();
   test_skip_channel_eof_message();
   test_skip_channel_eof_pmessage();

   test_skip_payload_not_string_message();
   test_skip_payload_not_string_pmessage();
   test_skip_payload_eof_message();
   test_skip_payload_eof_pmessage();

   test_skip_longer_message();
   test_skip_longer_pmessage();
   test_skip_longer_pmessage();

   test_only_skipped();
   test_valid_skipped();
   test_valid_skipped_mix();

   test_empty();

   test_range_interface();

   return boost::report_errors();
}

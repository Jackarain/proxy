/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/request.hpp>

#include <boost/assert/source_location.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/span.hpp>

#include <array>
#include <forward_list>
#include <iostream>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

using namespace boost::redis;
using detail::pubsub_change;
using detail::pubsub_change_type;

namespace {

// --- Utilities to check subscription tracking ---
const char* to_string(pubsub_change_type type)
{
   switch (type) {
      case pubsub_change_type::subscribe:    return "subscribe";
      case pubsub_change_type::unsubscribe:  return "unsubscribe";
      case pubsub_change_type::psubscribe:   return "psubscribe";
      case pubsub_change_type::punsubscribe: return "punsubscribe";
      default:                               return "<unknown pubsub_change_type>";
   }
}

// Like pubsub_change, but using a string instead of an offset
struct pubsub_change_str {
   pubsub_change_type type;
   std::string_view value;

   friend bool operator==(const pubsub_change_str& lhs, const pubsub_change_str& rhs)
   {
      return lhs.type == rhs.type && lhs.value == rhs.value;
   }

   friend std::ostream& operator<<(std::ostream& os, const pubsub_change_str& value)
   {
      return os << "{ " << to_string(value.type) << ", " << value.value << " }";
   }
};

void check_pubsub_changes(
   const request& req,
   boost::span<const pubsub_change_str> expected,
   boost::source_location loc = BOOST_CURRENT_LOCATION)
{
   // Convert from offsets to strings
   std::vector<pubsub_change_str> actual;
   for (const auto& change : detail::request_access::pubsub_changes(req)) {
      actual.push_back(
         {change.type, req.payload().substr(change.channel_offset, change.channel_size)});
   }

   // Check
   if (!BOOST_TEST_ALL_EQ(actual.begin(), actual.end(), expected.begin(), expected.end()))
      std::cerr << "Called from " << loc << std::endl;
}

// --- Generic functions to add commands ---
void test_push_no_args()
{
   request req1;
   req1.push("PING");
   BOOST_TEST_EQ(req1.payload(), "*1\r\n$4\r\nPING\r\n");
}

void test_push_int()
{
   request req;
   req.push("PING", 42);
   BOOST_TEST_EQ(req.payload(), "*2\r\n$4\r\nPING\r\n$2\r\n42\r\n");
}

void test_push_multiple_args()
{
   char const* res = "*5\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nvalue\r\n$2\r\nEX\r\n$1\r\n2\r\n";
   request req;
   req.push("SET", "key", "value", "EX", "2");
   BOOST_TEST_EQ(req.payload(), res);
}

// Subscription commands added with push are not tracked
void test_push_pubsub()
{
   request req;
   req.push("SUBSCRIBE", "ch1");
   req.push("UNSUBSCRIBE", "ch2");
   req.push("PSUBSCRIBE", "ch3*");
   req.push("PUNSUBSCRIBE", "ch4*");

   char const* res =
      "*2\r\n$9\r\nSUBSCRIBE\r\n$3\r\nch1\r\n"
      "*2\r\n$11\r\nUNSUBSCRIBE\r\n$3\r\nch2\r\n"
      "*2\r\n$10\r\nPSUBSCRIBE\r\n$4\r\nch3*\r\n"
      "*2\r\n$12\r\nPUNSUBSCRIBE\r\n$4\r\nch4*\r\n";
   BOOST_TEST_EQ(req.payload(), res);
   BOOST_TEST_EQ(req.get_expected_responses(), 0u);
   check_pubsub_changes(req, {});
}

// --- push_range ---
void test_push_range()
{
   std::map<std::string, std::string> in{
      {"key1", "value1"},
      {"key2", "value2"}
   };

   constexpr std::string_view expected =
      "*6\r\n$4\r\nHSET\r\n$3\r\nkey\r\n$4\r\nkey1\r\n$6\r\nvalue1\r\n$4\r\nkey2\r\n$"
      "6\r\nvalue2\r\n";

   request req1;
   req1.push_range("HSET", "key", in);
   BOOST_TEST_EQ(req1.payload(), expected);

   request req2;
   req2.push_range("HSET", "key", std::cbegin(in), std::cend(in));
   BOOST_TEST_EQ(req2.payload(), expected);
}

// Subscription commands added with push_range are not tracked
void test_push_range_pubsub()
{
   const std::vector<std::string_view> channels1{"ch1", "ch2"}, channels2{"ch3"}, patterns1{"ch3*"},
      patterns2{"ch4*"};
   request req;
   req.push_range("SUBSCRIBE", channels1);
   req.push_range("UNSUBSCRIBE", channels2);
   req.push_range("PSUBSCRIBE", patterns1);
   req.push_range("PUNSUBSCRIBE", patterns2);

   char const* res =
      "*3\r\n$9\r\nSUBSCRIBE\r\n$3\r\nch1\r\n$3\r\nch2\r\n"
      "*2\r\n$11\r\nUNSUBSCRIBE\r\n$3\r\nch3\r\n"
      "*2\r\n$10\r\nPSUBSCRIBE\r\n$4\r\nch3*\r\n"
      "*2\r\n$12\r\nPUNSUBSCRIBE\r\n$4\r\nch4*\r\n";
   BOOST_TEST_EQ(req.payload(), res);
   BOOST_TEST_EQ(req.get_expected_responses(), 0u);
   check_pubsub_changes(req, {});
}

// --- subscribe ---
// Most of the tests build the same request using different overloads.
// This fixture makes checking easier
struct subscribe_fixture {
   request req;

   void check_impl(
      std::string_view expected_payload,
      pubsub_change_type expected_type,
      boost::source_location loc = BOOST_CURRENT_LOCATION)
   {
      if (!BOOST_TEST_EQ(req.payload(), expected_payload))
         std::cerr << "Called from " << loc << std::endl;

      if (!BOOST_TEST_EQ(req.get_commands(), 1u))
         std::cerr << "Called from " << loc << std::endl;

      if (!BOOST_TEST_EQ(req.get_expected_responses(), 0u))
         std::cerr << "Called from " << loc << std::endl;

      const pubsub_change_str expected_changes[] = {
         {expected_type, "ch1"},
         {expected_type, "ch2"},
      };
      check_pubsub_changes(req, expected_changes, loc);
   }

   void check_subscribe(boost::source_location loc = BOOST_CURRENT_LOCATION)
   {
      check_impl(
         "*3\r\n$9\r\nSUBSCRIBE\r\n$3\r\nch1\r\n$3\r\nch2\r\n",
         pubsub_change_type::subscribe,
         loc);
   }

   void check_unsubscribe(boost::source_location loc = BOOST_CURRENT_LOCATION)
   {
      check_impl(
         "*3\r\n$11\r\nUNSUBSCRIBE\r\n$3\r\nch1\r\n$3\r\nch2\r\n",
         pubsub_change_type::unsubscribe,
         loc);
   }

   void check_psubscribe(boost::source_location loc = BOOST_CURRENT_LOCATION)
   {
      check_impl(
         "*3\r\n$10\r\nPSUBSCRIBE\r\n$3\r\nch1\r\n$3\r\nch2\r\n",
         pubsub_change_type::psubscribe,
         loc);
   }

   void check_punsubscribe(boost::source_location loc = BOOST_CURRENT_LOCATION)
   {
      check_impl(
         "*3\r\n$12\r\nPUNSUBSCRIBE\r\n$3\r\nch1\r\n$3\r\nch2\r\n",
         pubsub_change_type::punsubscribe,
         loc);
   }
};

void test_subscribe_iterators()
{
   subscribe_fixture fix;
   const std::forward_list<std::string_view> channels{"ch1", "ch2"};

   fix.req.subscribe(channels.begin(), channels.end());

   fix.check_subscribe();
}

// Like push_range, if the range is empty, this is a no-op
void test_subscribe_iterators_empty()
{
   const std::forward_list<std::string_view> channels;
   request req;

   req.subscribe(channels.begin(), channels.end());

   BOOST_TEST_EQ(req.payload(), "");
   BOOST_TEST_EQ(req.get_commands(), 0u);
   BOOST_TEST_EQ(req.get_expected_responses(), 0u);
   check_pubsub_changes(req, {});
}

// Iterators whose value_type is convertible to std::string_view work
void test_subscribe_iterators_convertible_string_view()
{
   subscribe_fixture fix;
   const std::vector<std::string> channels{"ch1", "ch2"};

   fix.req.subscribe(channels.begin(), channels.end());

   fix.check_subscribe();
}

// The range overload just dispatches to the iterator one
void test_subscribe_range()
{
   subscribe_fixture fix;
   const std::vector<std::string> channels{"ch1", "ch2"};

   fix.req.subscribe(channels);

   fix.check_subscribe();
}

// The initializer_list overload just dispatches to the iterator one
void test_subscribe_initializer_list()
{
   subscribe_fixture fix;

   fix.req.subscribe({"ch1", "ch2"});

   fix.check_subscribe();
}

// --- unsubscribe ---
void test_unsubscribe_iterators()
{
   subscribe_fixture fix;
   const std::forward_list<std::string_view> channels{"ch1", "ch2"};

   fix.req.unsubscribe(channels.begin(), channels.end());

   fix.check_unsubscribe();
}

// Like push_range, if the range is empty, this is a no-op
void test_unsubscribe_iterators_empty()
{
   const std::forward_list<std::string_view> channels;
   request req;

   req.unsubscribe(channels.begin(), channels.end());

   BOOST_TEST_EQ(req.payload(), "");
   BOOST_TEST_EQ(req.get_commands(), 0u);
   BOOST_TEST_EQ(req.get_expected_responses(), 0u);
   check_pubsub_changes(req, {});
}

// Iterators whose value_type is convertible to std::string_view work
void test_unsubscribe_iterators_convertible_string_view()
{
   subscribe_fixture fix;
   const std::vector<std::string> channels{"ch1", "ch2"};

   fix.req.unsubscribe(channels.begin(), channels.end());

   fix.check_unsubscribe();
}

// The range overload just dispatches to the iterator one
void test_unsubscribe_range()
{
   subscribe_fixture fix;
   const std::vector<std::string> channels{"ch1", "ch2"};

   fix.req.unsubscribe(channels);

   fix.check_unsubscribe();
}

// The initializer_list overload just dispatches to the iterator one
void test_unsubscribe_initializer_list()
{
   subscribe_fixture fix;

   fix.req.unsubscribe({"ch1", "ch2"});

   fix.check_unsubscribe();
}

// --- psubscribe ---
void test_psubscribe_iterators()
{
   subscribe_fixture fix;
   const std::forward_list<std::string_view> channels{"ch1", "ch2"};

   fix.req.psubscribe(channels.begin(), channels.end());

   fix.check_psubscribe();
}

// Like push_range, if the range is empty, this is a no-op
void test_psubscribe_iterators_empty()
{
   const std::forward_list<std::string_view> channels;
   request req;

   req.psubscribe(channels.begin(), channels.end());

   BOOST_TEST_EQ(req.payload(), "");
   BOOST_TEST_EQ(req.get_commands(), 0u);
   BOOST_TEST_EQ(req.get_expected_responses(), 0u);
   check_pubsub_changes(req, {});
}

// Iterators whose value_type is convertible to std::string_view work
void test_psubscribe_iterators_convertible_string_view()
{
   subscribe_fixture fix;
   const std::vector<std::string> channels{"ch1", "ch2"};

   fix.req.psubscribe(channels.begin(), channels.end());

   fix.check_psubscribe();
}

// The range overload just dispatches to the iterator one
void test_psubscribe_range()
{
   subscribe_fixture fix;
   const std::vector<std::string> channels{"ch1", "ch2"};

   fix.req.psubscribe(channels);

   fix.check_psubscribe();
}

// The initializer_list overload just dispatches to the iterator one
void test_psubscribe_initializer_list()
{
   subscribe_fixture fix;

   fix.req.psubscribe({"ch1", "ch2"});

   fix.check_psubscribe();
}

// --- punsubscribe ---
void test_punsubscribe_iterators()
{
   subscribe_fixture fix;
   const std::forward_list<std::string_view> channels{"ch1", "ch2"};

   fix.req.punsubscribe(channels.begin(), channels.end());

   fix.check_punsubscribe();
}

// Like push_range, if the range is empty, this is a no-op
void test_punsubscribe_iterators_empty()
{
   const std::forward_list<std::string_view> channels;
   request req;

   req.punsubscribe(channels.begin(), channels.end());

   BOOST_TEST_EQ(req.payload(), "");
   BOOST_TEST_EQ(req.get_commands(), 0u);
   BOOST_TEST_EQ(req.get_expected_responses(), 0u);
   check_pubsub_changes(req, {});
}

// Iterators whose value_type is convertible to std::string_view work
void test_punsubscribe_iterators_convertible_string_view()
{
   subscribe_fixture fix;
   const std::vector<std::string> channels{"ch1", "ch2"};

   fix.req.punsubscribe(channels.begin(), channels.end());

   fix.check_punsubscribe();
}

// The range overload just dispatches to the iterator one
void test_punsubscribe_range()
{
   subscribe_fixture fix;
   const std::vector<std::string> channels{"ch1", "ch2"};

   fix.req.punsubscribe(channels);

   fix.check_punsubscribe();
}

// The initializer_list overload just dispatches to the iterator one
void test_punsubscribe_initializer_list()
{
   subscribe_fixture fix;

   fix.req.punsubscribe({"ch1", "ch2"});

   fix.check_punsubscribe();
}

// Mixing regular commands and pubsub commands is OK
void test_mix_pubsub_regular()
{
   request req;
   req.push("PING");
   req.subscribe({"ch1", "ch2"});
   req.push("GET", "key");
   req.punsubscribe({"ch4*"});

   constexpr std::string_view expected =
      "*1\r\n$4\r\nPING\r\n"
      "*3\r\n$9\r\nSUBSCRIBE\r\n$3\r\nch1\r\n$3\r\nch2\r\n"
      "*2\r\n$3\r\nGET\r\n$3\r\nkey\r\n"
      "*2\r\n$12\r\nPUNSUBSCRIBE\r\n$4\r\nch4*\r\n";
   BOOST_TEST_EQ(req.payload(), expected);
   BOOST_TEST_EQ(req.get_commands(), 4u);
   BOOST_TEST_EQ(req.get_expected_responses(), 2u);
   constexpr pubsub_change_str expected_changes[] = {
      {pubsub_change_type::subscribe,    "ch1" },
      {pubsub_change_type::subscribe,    "ch2" },
      {pubsub_change_type::punsubscribe, "ch4*"},
   };
   check_pubsub_changes(req, expected_changes);
}

// --- hello ---
void test_hello()
{
   request req;
   req.hello();
   BOOST_TEST_EQ(req.payload(), "*2\r\n$5\r\nHELLO\r\n$1\r\n3\r\n");
}

void test_hello_auth()
{
   request req;
   req.hello("user", "pass");
   BOOST_TEST_EQ(
      req.payload(),
      "*5\r\n$5\r\nHELLO\r\n$1\r\n3\r\n$4\r\nAUTH\r\n$4\r\nuser\r\n$4\r\npass\r\n");
}

void test_hello_setname()
{
   request req;
   req.hello_setname("myclient");
   BOOST_TEST_EQ(
      req.payload(),
      "*4\r\n$5\r\nHELLO\r\n$1\r\n3\r\n$7\r\nSETNAME\r\n$8\r\nmyclient\r\n");
}

void test_hello_setname_auth()
{
   request req;
   req.hello_setname("user", "pass", "myclient");
   BOOST_TEST_EQ(
      req.payload(),
      "*7\r\n$5\r\nHELLO\r\n$1\r\n3\r\n$4\r\nAUTH\r\n$4\r\nuser\r\n$4\r\npass\r\n"
      "$7\r\nSETNAME\r\n$8\r\nmyclient\r\n");
}

// --- append ---
void test_append()
{
   request req1;
   req1.push("PING", "req1");

   request req2;
   req2.push("GET", "mykey");
   req2.push("GET", "other");

   req1.append(req2);

   constexpr std::string_view expected =
      "*2\r\n$4\r\nPING\r\n$4\r\nreq1\r\n"
      "*2\r\n$3\r\nGET\r\n$5\r\nmykey\r\n"
      "*2\r\n$3\r\nGET\r\n$5\r\nother\r\n";
   BOOST_TEST_EQ(req1.payload(), expected);
   BOOST_TEST_EQ(req1.get_commands(), 3u);
   BOOST_TEST_EQ(req1.get_expected_responses(), 3u);
   check_pubsub_changes(req1, {});
}

// Commands without responses are handled correctly
void test_append_no_response()
{
   request req1;
   req1.push("PING", "req1");

   request req2;
   req2.push("SUBSCRIBE", "mychannel");
   req2.push("GET", "other");

   req1.append(req2);

   constexpr std::string_view expected =
      "*2\r\n$4\r\nPING\r\n$4\r\nreq1\r\n"
      "*2\r\n$9\r\nSUBSCRIBE\r\n$9\r\nmychannel\r\n"
      "*2\r\n$3\r\nGET\r\n$5\r\nother\r\n";
   BOOST_TEST_EQ(req1.payload(), expected);
   BOOST_TEST_EQ(req1.get_commands(), 3u);
   BOOST_TEST_EQ(req1.get_expected_responses(), 2u);
   check_pubsub_changes(req1, {});
}

// Flags are not modified by append
void test_append_flags()
{
   request req1;
   req1.get_config().cancel_if_not_connected = false;
   req1.get_config().cancel_if_unresponded = false;
   req1.get_config().cancel_on_connection_lost = false;
   req1.push("PING", "req1");

   request req2;
   req2.get_config().cancel_if_not_connected = true;
   req2.get_config().cancel_if_unresponded = true;
   req2.get_config().cancel_on_connection_lost = true;
   req2.push("GET", "other");

   req1.append(req2);

   constexpr std::string_view expected =
      "*2\r\n$4\r\nPING\r\n$4\r\nreq1\r\n"
      "*2\r\n$3\r\nGET\r\n$5\r\nother\r\n";
   BOOST_TEST_EQ(req1.payload(), expected);
   BOOST_TEST_NOT(req1.get_config().cancel_if_not_connected);
   BOOST_TEST_NOT(req1.get_config().cancel_if_unresponded);
   BOOST_TEST_NOT(req1.get_config().cancel_on_connection_lost);
}

// Empty requests don't cause problems with append
void test_append_target_empty()
{
   request req1;

   request req2;
   req2.push("GET", "other");

   req1.append(req2);

   constexpr std::string_view expected = "*2\r\n$3\r\nGET\r\n$5\r\nother\r\n";
   BOOST_TEST_EQ(req1.payload(), expected);
   BOOST_TEST_EQ(req1.get_commands(), 1u);
   BOOST_TEST_EQ(req1.get_expected_responses(), 1u);
   check_pubsub_changes(req1, {});
}

void test_append_source_empty()
{
   request req1;
   req1.push("GET", "other");

   request req2;

   req1.append(req2);

   constexpr std::string_view expected = "*2\r\n$3\r\nGET\r\n$5\r\nother\r\n";
   BOOST_TEST_EQ(req1.payload(), expected);
   BOOST_TEST_EQ(req1.get_commands(), 1u);
   BOOST_TEST_EQ(req1.get_expected_responses(), 1u);
   check_pubsub_changes(req1, {});
}

void test_append_both_empty()
{
   request req1;
   request req2;

   req1.append(req2);

   BOOST_TEST_EQ(req1.payload(), "");
   BOOST_TEST_EQ(req1.get_commands(), 0u);
   BOOST_TEST_EQ(req1.get_expected_responses(), 0u);
   check_pubsub_changes(req1, {});
}

// Append correctly handles requests with pubsub changes
void test_append_pubsub()
{
   request req1;
   req1.subscribe({"ch1"});

   auto req2 = std::make_unique<request>();
   req2->unsubscribe({"ch2"});
   req2->psubscribe({"really_very_long_pattern_name*"});

   req1.append(*req2);
   req2.reset();  // make sure we don't leave dangling pointers

   constexpr std::string_view expected =
      "*2\r\n$9\r\nSUBSCRIBE\r\n$3\r\nch1\r\n"
      "*2\r\n$11\r\nUNSUBSCRIBE\r\n$3\r\nch2\r\n"
      "*2\r\n$10\r\nPSUBSCRIBE\r\n$30\r\nreally_very_long_pattern_name*\r\n";
   BOOST_TEST_EQ(req1.payload(), expected);
   const pubsub_change_str expected_changes[] = {
      {pubsub_change_type::subscribe,   "ch1"                           },
      {pubsub_change_type::unsubscribe, "ch2"                           },
      {pubsub_change_type::psubscribe,  "really_very_long_pattern_name*"},
   };
   check_pubsub_changes(req1, expected_changes);
}

// If the target is empty and the source has pubsub changes, that's OK
void test_append_pubsub_target_empty()
{
   request req1;

   request req2;
   req2.punsubscribe({"ch2"});

   req1.append(req2);

   constexpr std::string_view expected = "*2\r\n$12\r\nPUNSUBSCRIBE\r\n$3\r\nch2\r\n";
   BOOST_TEST_EQ(req1.payload(), expected);
   const pubsub_change_str expected_changes[] = {
      {pubsub_change_type::punsubscribe, "ch2"},
   };
   check_pubsub_changes(req1, expected_changes);
}

// --- clear ---
void test_clear()
{
   // Create  request with some commands and some pubsub changes
   request req;
   req.push("PING", "value");
   req.push("GET", "key");
   req.subscribe({"ch1", "ch2"});
   req.punsubscribe({"ch3*"});

   // Clear removes the payload, the commands and the pubsub changes
   req.clear();
   BOOST_TEST_EQ(req.payload(), "");
   BOOST_TEST_EQ(req.get_commands(), 0u);
   BOOST_TEST_EQ(req.get_expected_responses(), 0u);
   check_pubsub_changes(req, {});

   // Clearing again does nothing
   req.clear();
   BOOST_TEST_EQ(req.payload(), "");
   BOOST_TEST_EQ(req.get_commands(), 0u);
   BOOST_TEST_EQ(req.get_expected_responses(), 0u);
   check_pubsub_changes(req, {});
}

// Clearing an empty request doesn't cause trouble
void test_clear_empty()
{
   request req;

   req.clear();

   BOOST_TEST_EQ(req.payload(), "");
   BOOST_TEST_EQ(req.get_commands(), 0u);
   BOOST_TEST_EQ(req.get_expected_responses(), 0u);
   check_pubsub_changes(req, {});
}

}  // namespace

int main()
{
   test_push_no_args();
   test_push_int();
   test_push_multiple_args();
   test_push_pubsub();

   test_push_range();
   test_push_range_pubsub();

   test_subscribe_iterators();
   test_subscribe_iterators_empty();
   test_subscribe_iterators_convertible_string_view();
   test_subscribe_range();
   test_subscribe_initializer_list();

   test_unsubscribe_iterators();
   test_unsubscribe_iterators_empty();
   test_unsubscribe_iterators_convertible_string_view();
   test_unsubscribe_range();
   test_unsubscribe_initializer_list();

   test_psubscribe_iterators();
   test_psubscribe_iterators_empty();
   test_psubscribe_iterators_convertible_string_view();
   test_psubscribe_range();
   test_psubscribe_initializer_list();

   test_punsubscribe_iterators();
   test_punsubscribe_iterators_empty();
   test_punsubscribe_iterators_convertible_string_view();
   test_punsubscribe_range();
   test_punsubscribe_initializer_list();

   test_mix_pubsub_regular();

   test_hello();
   test_hello_auth();
   test_hello_setname();
   test_hello_setname_auth();

   test_append();
   test_append_no_response();
   test_append_flags();
   test_append_target_empty();
   test_append_source_empty();
   test_append_both_empty();
   test_append_pubsub();
   test_append_pubsub_target_empty();

   test_clear();
   test_clear_empty();

   return boost::report_errors();
}

/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/adapter/adapt.hpp>
#include <boost/redis/resp3/flat_tree.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/resp3/parser.hpp>
#include <boost/redis/resp3/type.hpp>

#include <boost/assert/source_location.hpp>
#include <boost/config.hpp>  // for a safe #include <version>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/span.hpp>

#include "print_node.hpp"

#include <algorithm>
#include <array>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#if (__cpp_lib_ranges >= 201911L) && (__cpp_lib_concepts >= 202002L)
#define BOOST_REDIS_TEST_RANGE_CONCEPTS
#include <ranges>
#endif

using boost::redis::adapter::adapt2;
using boost::redis::adapter::result;
using boost::redis::resp3::tree;
using boost::redis::resp3::flat_tree;
using boost::redis::generic_flat_response;
using boost::redis::resp3::type;
using boost::redis::resp3::detail::deserialize;
using boost::redis::resp3::node;
using boost::redis::resp3::node_view;
using boost::redis::resp3::parser;
using boost::redis::resp3::to_string;
using boost::redis::response;
using boost::system::error_code;

namespace {

void add_nodes(
   flat_tree& to,
   std::string_view data,
   boost::source_location loc = BOOST_CURRENT_LOCATION)
{
   error_code ec;
   deserialize(data, adapt2(to), ec);
   if (!BOOST_TEST_EQ(ec, error_code{}))
      std::cerr << "Called from " << loc << std::endl;
}

bool parse_checked(
   flat_tree& to,
   parser& p,
   std::string_view data,
   boost::source_location loc = BOOST_CURRENT_LOCATION)
{
   error_code ec;
   auto adapter = adapt2(to);
   bool done = boost::redis::resp3::parse(p, data, adapter, ec);
   if (!BOOST_TEST_EQ(ec, error_code{}))
      std::cerr << "Called from " << loc << std::endl;
   return done;
}

void check_nodes(
   const flat_tree& tree,
   boost::span<const node_view> expected,
   boost::source_location loc = BOOST_CURRENT_LOCATION)
{
   if (!BOOST_TEST_ALL_EQ(tree.begin(), tree.end(), expected.begin(), expected.end()))
      std::cerr << "Called from " << loc << std::endl;
}

// --- Adding nodes ---
// Adding nodes works, even when reallocations happen.
// Empty nodes don't cause trouble
void test_add_nodes()
{
   flat_tree t;

   // Add a bunch of nodes. Single allocation. Some nodes are empty.
   add_nodes(t, "*2\r\n+hello\r\n+world\r\n");
   std::vector<node_view> expected_nodes{
      {type::array,         2u, 0u, ""     },
      {type::simple_string, 1u, 1u, "hello"},
      {type::simple_string, 1u, 1u, "world"},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 10u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);
   BOOST_TEST_EQ(t.get_reallocs(), 1u);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);

   // Capacity will have raised to 512 bytes, at least. Add some more without reallocations
   add_nodes(t, "$3\r\nbye\r\n");
   expected_nodes.push_back({type::blob_string, 1u, 0u, "bye"});
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 13u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);
   BOOST_TEST_EQ(t.get_reallocs(), 1u);
   BOOST_TEST_EQ(t.get_total_msgs(), 2u);

   // Add nodes above the first reallocation threshold. Node strings are still valid
   const std::string long_value(600u, 'a');
   add_nodes(t, "+" + long_value + "\r\n");
   expected_nodes.push_back({type::simple_string, 1u, 0u, long_value});
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 613u);
   BOOST_TEST_EQ(t.data_capacity(), 1024u);
   BOOST_TEST_EQ(t.get_reallocs(), 2u);
   BOOST_TEST_EQ(t.get_total_msgs(), 3u);

   // Add some more nodes, still within the reallocation threshold
   add_nodes(t, "+some_other_value\r\n");
   expected_nodes.push_back({type::simple_string, 1u, 0u, "some_other_value"});
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 629u);
   BOOST_TEST_EQ(t.data_capacity(), 1024u);
   BOOST_TEST_EQ(t.get_reallocs(), 2u);
   BOOST_TEST_EQ(t.get_total_msgs(), 4u);

   // Add some more, causing another reallocation
   add_nodes(t, "+" + long_value + "\r\n");
   expected_nodes.push_back({type::simple_string, 1u, 0u, long_value});
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 1229u);
   BOOST_TEST_EQ(t.data_capacity(), 2048u);
   BOOST_TEST_EQ(t.get_reallocs(), 3u);
   BOOST_TEST_EQ(t.get_total_msgs(), 5u);
}

// Strings are really copied into the object
void test_add_nodes_copies()
{
   flat_tree t;

   // Place the message in dynamic memory
   constexpr std::string_view const_msg = "+some_long_value_for_a_node\r\n";
   std::unique_ptr<char[]> data{new char[100]{}};
   std::copy(const_msg.begin(), const_msg.end(), data.get());

   // Add nodes pointing into this message
   add_nodes(t, data.get());

   // Invalidate the original message
   data.reset();

   // Check
   std::vector<node_view> expected_nodes{
      {type::simple_string, 1u, 0u, "some_long_value_for_a_node"},
   };
   check_nodes(t, expected_nodes);
}

// Reallocations happen only when we would exceed capacity
void test_add_nodes_capacity_limit()
{
   flat_tree t;

   // Add a node to reach capacity 512
   add_nodes(t, "+hello\r\n");
   BOOST_TEST_EQ(t.data_size(), 5u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);

   // Fill the rest of the capacity
   add_nodes(t, "+" + std::string(507u, 'b') + "\r\n");
   BOOST_TEST_EQ(t.data_size(), 512u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);

   // Adding an empty node here doesn't change capacity
   add_nodes(t, "_\r\n");
   BOOST_TEST_EQ(t.data_size(), 512u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);

   // Adding more data causes a reallocation
   add_nodes(t, "+a\r\n");
   BOOST_TEST_EQ(t.data_size(), 513u);
   BOOST_TEST_EQ(t.data_capacity(), 1024);

   // Same goes for the next capacity limit
   add_nodes(t, "+" + std::string(511u, 'c') + "\r\n");
   BOOST_TEST_EQ(t.data_size(), 1024);
   BOOST_TEST_EQ(t.data_capacity(), 1024);

   // Reallocation
   add_nodes(t, "+u\r\n");
   BOOST_TEST_EQ(t.data_size(), 1025u);
   BOOST_TEST_EQ(t.data_capacity(), 2048u);

   // This would continue
   add_nodes(t, "+" + std::string(1024u, 'd') + "\r\n");
   BOOST_TEST_EQ(t.data_size(), 2049u);
   BOOST_TEST_EQ(t.data_capacity(), 4096u);
}

// It's no problem if a node is big enough to surpass several reallocation limits
void test_add_nodes_big_node()
{
   flat_tree t;

   // Add a bunch of nodes. Single allocation. Some nodes are empty.
   const std::string long_value(1500u, 'h');
   add_nodes(t, "+" + long_value + "\r\n");
   std::vector<node_view> expected_nodes{
      {type::simple_string, 1u, 0u, long_value},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 1500u);
   BOOST_TEST_EQ(t.data_capacity(), 2048u);
   BOOST_TEST_EQ(t.get_reallocs(), 1u);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);
}

// Flat trees have a temporary area (tmp) where nodes are stored while
// messages are being parsed. Nodes in the tmp area are not part of the representation
// until they are committed when the message has been fully parsed
void test_add_nodes_tmp()
{
   flat_tree t;
   parser p;

   // Add part of a message, but not all of it.
   // These nodes are stored but are not part of the user-facing representation
   BOOST_TEST_NOT(parse_checked(t, p, "*2\r\n+hello\r\n"));
   check_nodes(t, {});
   BOOST_TEST_EQ(t.data_size(), 0u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);
   BOOST_TEST_EQ(t.get_total_msgs(), 0u);

   // Finish the message. Nodes will now show up
   BOOST_TEST(parse_checked(t, p, "*2\r\n+hello\r\n+world\r\n"));
   std::vector<node_view> expected_nodes{
      {type::array,         2u, 0u, ""     },
      {type::simple_string, 1u, 1u, "hello"},
      {type::simple_string, 1u, 1u, "world"},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 10u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);

   // We can repeat this cycle again
   p.reset();
   BOOST_TEST_NOT(parse_checked(t, p, ">2\r\n+good\r\n"));
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 10u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);

   BOOST_TEST(parse_checked(t, p, ">2\r\n+good\r\n+bye\r\n"));
   expected_nodes.push_back({type::push, 2u, 0u, ""});
   expected_nodes.push_back({type::simple_string, 1u, 1u, "good"});
   expected_nodes.push_back({type::simple_string, 1u, 1u, "bye"});
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 17u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);
   BOOST_TEST_EQ(t.get_total_msgs(), 2u);
}

// If there was an unfinished message when another message is started,
// the former is discarded
void test_add_nodes_existing_tmp()
{
   flat_tree t;
   parser p;

   // Add part of a message
   BOOST_TEST_NOT(parse_checked(t, p, ">3\r\n+some message\r\n"));
   check_nodes(t, {});
   BOOST_TEST_EQ(t.data_size(), 0u);
   BOOST_TEST_EQ(t.get_total_msgs(), 0u);

   // This message is abandoned, and another one is started
   p.reset();
   BOOST_TEST_NOT(parse_checked(t, p, "%66\r\n+abandoned\r\n"));
   check_nodes(t, {});
   BOOST_TEST_EQ(t.data_size(), 0u);
   BOOST_TEST_EQ(t.get_total_msgs(), 0u);

   // This happens again, but this time a complete message is added
   add_nodes(t, "*2\r\n+hello\r\n+world\r\n");
   std::vector<node_view> expected_nodes{
      {type::array,         2u, 0u, ""     },
      {type::simple_string, 1u, 1u, "hello"},
      {type::simple_string, 1u, 1u, "world"},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 10u);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);
}

// The same works even if there is existing committed data
void test_add_nodes_existing_data_and_tmp()
{
   flat_tree t;
   parser p;

   // Add a full message
   add_nodes(t, "*2\r\n+hello\r\n+world\r\n");
   std::vector<node_view> expected_nodes{
      {type::array,         2u, 0u, ""     },
      {type::simple_string, 1u, 1u, "hello"},
      {type::simple_string, 1u, 1u, "world"},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 10u);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);

   // Add part of a message
   p.reset();
   BOOST_TEST_NOT(parse_checked(t, p, "%66\r\n+abandoned\r\n"));
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 10u);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);

   // This message is abandoned, and replaced by a full one
   add_nodes(t, "+complete message\r\n");
   expected_nodes.push_back({type::simple_string, 1u, 0u, "complete message"});
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 26u);
   BOOST_TEST_EQ(t.get_total_msgs(), 2u);
}

// --- Reserving space ---
// The usual case, calling it before using it
void test_reserve()
{
   flat_tree t;

   t.reserve(1024u, 5u);
   check_nodes(t, {});
   BOOST_TEST_GE(t.capacity(), 5u);
   BOOST_TEST_EQ(t.data_size(), 0u);
   BOOST_TEST_EQ(t.data_capacity(), 1024);
   BOOST_TEST_EQ(t.get_reallocs(), 1u);
   BOOST_TEST_EQ(t.get_total_msgs(), 0u);

   // Adding some nodes now works
   add_nodes(t, "+hello\r\n");
   std::vector<node_view> expected_nodes{
      {type::simple_string, 1u, 0u, "hello"},
   };
   check_nodes(t, expected_nodes);
}

// Reserving space uses the same allocation thresholds
void test_reserve_not_power_of_2()
{
   flat_tree t;

   // First threshold at 512
   t.reserve(200u, 5u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);
   BOOST_TEST_EQ(t.get_reallocs(), 1u);

   // Second threshold at 1024
   t.reserve(600u, 5u);
   BOOST_TEST_EQ(t.data_capacity(), 1024u);
   BOOST_TEST_EQ(t.get_reallocs(), 2u);
}

// Requesting a capacity below the current one does nothing
void test_reserve_below_current_capacity()
{
   flat_tree t;

   // Reserving with a zero capacity does nothing
   t.reserve(0u, 0u);
   BOOST_TEST_EQ(t.data_capacity(), 0u);
   BOOST_TEST_EQ(t.get_reallocs(), 0u);

   // Increase capacity
   t.reserve(400u, 5u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);
   BOOST_TEST_EQ(t.get_reallocs(), 1u);

   // Reserving again does nothing
   t.reserve(400u, 5u);
   t.reserve(512u, 5u);
   t.reserve(0u, 5u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);
   BOOST_TEST_EQ(t.get_reallocs(), 1u);
}

// Reserving might reallocate. If there are nodes, strings remain valid
void test_reserve_with_data()
{
   flat_tree t;

   // Add a bunch of nodes, and then reserve
   add_nodes(t, "*2\r\n+hello\r\n+world\r\n");
   t.reserve(1000u, 10u);

   // Check
   std::vector<node_view> expected_nodes{
      {type::array,         2u, 0u, ""     },
      {type::simple_string, 1u, 1u, "hello"},
      {type::simple_string, 1u, 1u, "world"},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 10u);
   BOOST_TEST_EQ(t.data_capacity(), 1024u);
   BOOST_TEST_EQ(t.get_reallocs(), 2u);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);
}

// Reserve also handles the tmp area
void test_reserve_with_tmp()
{
   flat_tree t;
   parser p;

   // Add a partial message, and then reserve
   BOOST_TEST_NOT(parse_checked(t, p, "*2\r\n+hello\r\n"));
   t.reserve(1000u, 10u);

   // Finish the current message so nodes in the tmp area show up
   BOOST_TEST(parse_checked(t, p, "*2\r\n+hello\r\n+world\r\n"));

   // Check
   std::vector<node_view> expected_nodes{
      {type::array,         2u, 0u, ""     },
      {type::simple_string, 1u, 1u, "hello"},
      {type::simple_string, 1u, 1u, "world"},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 10u);
   BOOST_TEST_EQ(t.data_capacity(), 1024u);
   BOOST_TEST_EQ(t.get_reallocs(), 2u);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);
}

// --- Clear ---
void test_clear()
{
   flat_tree t;

   // Add a bunch of nodes, then clear
   add_nodes(t, "*2\r\n+hello\r\n+world\r\n");
   t.clear();

   // Nodes are no longer there, but memory hasn't been fred
   check_nodes(t, {});
   BOOST_TEST_EQ(t.data_size(), 0u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);
   BOOST_TEST_EQ(t.get_reallocs(), 1u);
   BOOST_TEST_EQ(t.get_total_msgs(), 0u);
}

// Clearing an empty tree doesn't cause trouble
void test_clear_empty()
{
   flat_tree t;
   t.clear();

   check_nodes(t, {});
   BOOST_TEST_EQ(t.data_size(), 0u);
   BOOST_TEST_EQ(t.data_capacity(), 0u);
   BOOST_TEST_EQ(t.get_reallocs(), 0u);
   BOOST_TEST_EQ(t.get_total_msgs(), 0u);
}

// With clear, memory can be reused
// The response should be reusable.
void test_clear_reuse()
{
   flat_tree t;

   // First use
   add_nodes(t, "~6\r\n+orange\r\n+apple\r\n+one\r\n+two\r\n+three\r\n+orange\r\n");
   std::vector<node_view> expected_nodes{
      {type::set,           6u, 0u, ""      },
      {type::simple_string, 1u, 1u, "orange"},
      {type::simple_string, 1u, 1u, "apple" },
      {type::simple_string, 1u, 1u, "one"   },
      {type::simple_string, 1u, 1u, "two"   },
      {type::simple_string, 1u, 1u, "three" },
      {type::simple_string, 1u, 1u, "orange"},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.get_reallocs(), 1u);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);

   // Second use
   t.clear();
   add_nodes(t, "*2\r\n+hello\r\n+world\r\n");
   expected_nodes = {
      {type::array,         2u, 0u, ""     },
      {type::simple_string, 1u, 1u, "hello"},
      {type::simple_string, 1u, 1u, "world"},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.get_reallocs(), 1u);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);
}

// Clear doesn't remove the tmp area
void test_clear_tmp()
{
   flat_tree t;
   parser p;

   // Add a full message and part of another
   add_nodes(t, ">2\r\n+orange\r\n+apple\r\n");
   BOOST_TEST_NOT(parse_checked(t, p, "*2\r\n+hello\r\n"));
   std::vector<node_view> expected_nodes{
      {type::push,          2u, 0u, ""      },
      {type::simple_string, 1u, 1u, "orange"},
      {type::simple_string, 1u, 1u, "apple" },
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);

   // Clearing removes the user-facing representation
   t.clear();
   check_nodes(t, {});
   BOOST_TEST_EQ(t.get_total_msgs(), 0u);

   // The nodes in the tmp area are still alive. Adding the remaining yields the full message
   BOOST_TEST(parse_checked(t, p, "*2\r\n+hello\r\n+world\r\n"));
   expected_nodes = {
      {type::array,         2u, 0u, ""     },
      {type::simple_string, 1u, 1u, "hello"},
      {type::simple_string, 1u, 1u, "world"},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);
}

// Clearing having only tmp area is safe
void test_clear_only_tmp()
{
   flat_tree t;
   parser p;

   // Add part of a message
   BOOST_TEST_NOT(parse_checked(t, p, "*2\r\n+hello\r\n"));
   check_nodes(t, {});
   BOOST_TEST_EQ(t.get_total_msgs(), 0u);

   // Clearing here does nothing
   t.clear();
   check_nodes(t, {});
   BOOST_TEST_EQ(t.get_total_msgs(), 0u);

   // The nodes in the tmp area are still alive. Adding the remaining yields the full message
   BOOST_TEST(parse_checked(t, p, "*2\r\n+hello\r\n+world\r\n"));
   std::vector<node_view> expected_nodes = {
      {type::array,         2u, 0u, ""     },
      {type::simple_string, 1u, 1u, "hello"},
      {type::simple_string, 1u, 1u, "world"},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);
}

// Clearing having tmp nodes but no data is also safe
void test_clear_only_tmp_nodes()
{
   flat_tree t;
   parser p;

   // Add part of a message
   BOOST_TEST_NOT(parse_checked(t, p, "*2\r\n"));
   check_nodes(t, {});
   BOOST_TEST_EQ(t.get_total_msgs(), 0u);

   // Clearing here does nothing
   t.clear();
   check_nodes(t, {});
   BOOST_TEST_EQ(t.get_total_msgs(), 0u);

   // The nodes in the tmp area are still alive. Adding the remaining yields the full message
   BOOST_TEST(parse_checked(t, p, "*2\r\n+hello\r\n+world\r\n"));
   std::vector<node_view> expected_nodes = {
      {type::array,         2u, 0u, ""     },
      {type::simple_string, 1u, 1u, "hello"},
      {type::simple_string, 1u, 1u, "world"},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);
}

// --- Default ctor ---
void test_default_constructor()
{
   flat_tree t;

   check_nodes(t, {});
   BOOST_TEST_EQ(t.data_size(), 0u);
   BOOST_TEST_EQ(t.get_reallocs(), 0u);
   BOOST_TEST_EQ(t.get_total_msgs(), 0u);
}

// --- Copy ctor ---
void test_copy_ctor()
{
   // Setup
   auto t = std::make_unique<flat_tree>();
   add_nodes(*t, "*2\r\n+hello\r\n+world\r\n");
   std::vector<node_view> expected_nodes{
      {type::array,         2u, 0u, ""     },
      {type::simple_string, 1u, 1u, "hello"},
      {type::simple_string, 1u, 1u, "world"},
   };

   // Construct, then destroy the original copy
   flat_tree t2{*t};
   t.reset();

   // Check
   check_nodes(t2, expected_nodes);
   BOOST_TEST_EQ(t2.data_size(), 10u);
   BOOST_TEST_EQ(t2.data_capacity(), 512u);
   BOOST_TEST_EQ(t2.get_reallocs(), 1u);
   BOOST_TEST_EQ(t2.get_total_msgs(), 1u);
}

// Copying an empty tree doesn't cause problems
void test_copy_ctor_empty()
{
   flat_tree t;
   flat_tree t2{t};
   check_nodes(t2, {});
   BOOST_TEST_EQ(t2.data_size(), 0u);
   BOOST_TEST_EQ(t2.data_capacity(), 0u);
   BOOST_TEST_EQ(t2.get_reallocs(), 0u);
   BOOST_TEST_EQ(t2.get_total_msgs(), 0u);
}

// Copying an object that has no elements but some capacity doesn't cause trouble
void test_copy_ctor_empty_with_capacity()
{
   flat_tree t;
   t.reserve(300u, 8u);

   flat_tree t2{t};
   check_nodes(t2, {});
   BOOST_TEST_EQ(t2.data_size(), 0u);
   BOOST_TEST_EQ(t2.data_capacity(), 0u);
   BOOST_TEST_EQ(t2.get_reallocs(), 0u);
   BOOST_TEST_EQ(t2.get_total_msgs(), 0u);
}

// Copying an object with more capacity than required adjusts its capacity
void test_copy_ctor_adjust_capacity()
{
   // Setup
   flat_tree t;
   add_nodes(t, "+hello\r\n");
   std::vector<node_view> expected_nodes{
      {type::simple_string, 1u, 0u, "hello"},
   };

   // Cause reallocations
   t.reserve(1000u, 10u);
   t.reserve(2000u, 10u);
   t.reserve(4000u, 10u);

   // Copy
   flat_tree t2{t};

   // The target object has the minimum required capacity,
   // and the number of reallocs has been reset
   check_nodes(t2, expected_nodes);
   BOOST_TEST_EQ(t2.data_size(), 5u);
   BOOST_TEST_EQ(t2.data_capacity(), 512u);
   BOOST_TEST_EQ(t2.get_reallocs(), 1u);
   BOOST_TEST_EQ(t2.get_total_msgs(), 1u);
}

// Copying an object also copies its tmp area
void test_copy_ctor_tmp()
{
   // Setup
   flat_tree t;
   parser p;
   add_nodes(t, "+message\r\n");
   BOOST_TEST_NOT(parse_checked(t, p, "*2\r\n+hello\r\n"));
   std::vector<node_view> expected_nodes{
      {type::simple_string, 1u, 0u, "message"},
   };

   // Copy. The copy has the tmp nodes but they're hidden in its tmp area
   flat_tree t2{t};
   check_nodes(t2, expected_nodes);
   BOOST_TEST_EQ(t2.data_size(), 7u);
   BOOST_TEST_EQ(t2.get_total_msgs(), 1u);

   // Finishing the message in the copy works
   BOOST_TEST(parse_checked(t2, p, "*2\r\n+hello\r\n+world\r\n"));
   expected_nodes = {
      {type::simple_string, 1u, 0u, "message"},
      {type::array,         2u, 0u, ""       },
      {type::simple_string, 1u, 1u, "hello"  },
      {type::simple_string, 1u, 1u, "world"  },
   };
   check_nodes(t2, expected_nodes);
   BOOST_TEST_EQ(t2.data_size(), 17u);
   BOOST_TEST_EQ(t2.get_total_msgs(), 2u);
}

// --- Move ctor ---
void test_move_ctor()
{
   flat_tree t;
   add_nodes(t, "*2\r\n+hello\r\n+world\r\n");

   flat_tree t2{std::move(t)};

   std::vector<node_view> expected_nodes{
      {type::array,         2u, 0u, ""     },
      {type::simple_string, 1u, 1u, "hello"},
      {type::simple_string, 1u, 1u, "world"},
   };
   check_nodes(t2, expected_nodes);
   BOOST_TEST_EQ(t2.data_size(), 10u);
   BOOST_TEST_EQ(t2.data_capacity(), 512u);
   BOOST_TEST_EQ(t2.get_reallocs(), 1u);
   BOOST_TEST_EQ(t2.get_total_msgs(), 1u);
}

// Moving an empty object doesn't cause trouble
void test_move_ctor_empty()
{
   flat_tree t;

   flat_tree t2{std::move(t)};

   check_nodes(t2, {});
   BOOST_TEST_EQ(t2.data_size(), 0u);
   BOOST_TEST_EQ(t2.data_capacity(), 0u);
   BOOST_TEST_EQ(t2.get_reallocs(), 0u);
   BOOST_TEST_EQ(t2.get_total_msgs(), 0u);
}

// Moving an object with capacity but no data doesn't cause trouble
void test_move_ctor_with_capacity()
{
   flat_tree t;
   t.reserve(1000u, 10u);

   flat_tree t2{std::move(t)};

   check_nodes(t2, {});
   BOOST_TEST_EQ(t2.data_size(), 0u);
   BOOST_TEST_EQ(t2.data_capacity(), 1024u);
   BOOST_TEST_EQ(t2.get_reallocs(), 1u);
   BOOST_TEST_EQ(t2.get_total_msgs(), 0u);
}

// Moving an object also moves its tmp area
void test_move_ctor_tmp()
{
   // Setup
   flat_tree t;
   parser p;
   add_nodes(t, "+message\r\n");
   BOOST_TEST_NOT(parse_checked(t, p, "*2\r\n+hello\r\n"));
   std::vector<node_view> expected_nodes{
      {type::simple_string, 1u, 0u, "message"},
   };

   // Move. The new object has the same tmp area
   flat_tree t2{std::move(t)};
   check_nodes(t2, expected_nodes);
   BOOST_TEST_EQ(t2.data_size(), 7u);
   BOOST_TEST_EQ(t2.get_total_msgs(), 1u);

   // Finishing the message in the copy works
   BOOST_TEST(parse_checked(t2, p, "*2\r\n+hello\r\n+world\r\n"));
   expected_nodes = {
      {type::simple_string, 1u, 0u, "message"},
      {type::array,         2u, 0u, ""       },
      {type::simple_string, 1u, 1u, "hello"  },
      {type::simple_string, 1u, 1u, "world"  },
   };
   check_nodes(t2, expected_nodes);
   BOOST_TEST_EQ(t2.data_size(), 17u);
   BOOST_TEST_EQ(t2.get_total_msgs(), 2u);
}

// --- Copy assignment ---
void test_copy_assign()
{
   flat_tree t;
   add_nodes(t, "+some_data\r\n");

   auto t2 = std::make_unique<flat_tree>();
   add_nodes(*t2, "*2\r\n+hello\r\n+world\r\n");

   t = *t2;

   // Delete the source object, to check that we copied the contents
   t2.reset();

   std::vector<node_view> expected_nodes{
      {type::array,         2u, 0u, ""     },
      {type::simple_string, 1u, 1u, "hello"},
      {type::simple_string, 1u, 1u, "world"},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 10u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);
   BOOST_TEST_EQ(t.get_reallocs(), 1u);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);
}

// The lhs is empty and doesn't have any capacity
void test_copy_assign_target_empty()
{
   flat_tree t;

   flat_tree t2;
   add_nodes(t2, "+hello\r\n");

   t = t2;

   std::vector<node_view> expected_nodes{
      {type::simple_string, 1u, 0u, "hello"},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 5u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);
   BOOST_TEST_EQ(t.get_reallocs(), 1u);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);
}

// If the target doesn't have enough capacity, a reallocation happens
void test_copy_assign_target_not_enough_capacity()
{
   flat_tree t;
   add_nodes(t, "+hello\r\n");

   const std::string big_node(2000u, 'a');
   flat_tree t2;
   add_nodes(t2, "+" + big_node + "\r\n");

   t = t2;

   std::vector<node_view> expected_nodes{
      {type::simple_string, 1u, 0u, big_node},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 2000u);
   BOOST_TEST_EQ(t.data_capacity(), 2048u);
   BOOST_TEST_EQ(t.get_reallocs(), 2u);  // initial + assignment
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);
}

// If the source of the assignment is empty, nothing bad happens
void test_copy_assign_source_empty()
{
   flat_tree t;
   add_nodes(t, "+hello\r\n");

   flat_tree t2;

   t = t2;

   check_nodes(t, {});
   BOOST_TEST_EQ(t.data_size(), 0u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);  // capacity is kept
   BOOST_TEST_EQ(t.get_reallocs(), 1u);
   BOOST_TEST_EQ(t.get_total_msgs(), 0u);
}

// If the source of the assignment has capacity but no data, we're OK
void test_copy_assign_source_with_capacity()
{
   flat_tree t;
   add_nodes(t, "+hello\r\n");

   flat_tree t2;
   t2.reserve(1000u, 4u);
   t2.reserve(4000u, 8u);

   t = t2;

   check_nodes(t, {});
   BOOST_TEST_EQ(t.data_size(), 0u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);  // capacity is kept
   BOOST_TEST_EQ(t.get_reallocs(), 1u);     // not propagated
   BOOST_TEST_EQ(t.get_total_msgs(), 0u);
}

// If the source of the assignment has data with extra capacity
// and a reallocation is needed, the minimum amount of space is allocated
void test_copy_assign_source_with_extra_capacity()
{
   flat_tree t;

   flat_tree t2;
   add_nodes(t2, "+hello\r\n");
   t2.reserve(4000u, 8u);

   t = t2;

   std::vector<node_view> expected_nodes{
      {type::simple_string, 1u, 0u, "hello"},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 5u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);
   BOOST_TEST_EQ(t.get_reallocs(), 1u);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);
}

void test_copy_assign_both_empty()
{
   flat_tree t;
   flat_tree t2;

   t = t2;

   check_nodes(t, {});
   BOOST_TEST_EQ(t.data_size(), 0u);
   BOOST_TEST_EQ(t.data_capacity(), 0u);
   BOOST_TEST_EQ(t.get_reallocs(), 0u);
   BOOST_TEST_EQ(t.get_total_msgs(), 0u);
}

// Self-assignment doesn't cause trouble
void test_copy_assign_self()
{
   flat_tree t;
   add_nodes(t, "+hello\r\n");

   const auto& tref = t;
   t = tref;

   std::vector<node_view> expected_nodes{
      {type::simple_string, 1u, 0u, "hello"},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 5u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);
   BOOST_TEST_EQ(t.get_reallocs(), 1u);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);
}

// Copy assignment also assigns the tmp area
void test_copy_assign_tmp()
{
   parser p;

   flat_tree t;
   add_nodes(t, "+some_data\r\n");

   flat_tree t2;
   add_nodes(t2, "+message\r\n");
   BOOST_TEST_NOT(parse_checked(t2, p, "*2\r\n+hello\r\n"));

   // Assigning also copies where the tmp area starts
   t = t2;
   std::vector<node_view> expected_nodes{
      {type::simple_string, 1u, 0u, "message"},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 7u);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);

   // The tmp area was also copied
   BOOST_TEST(parse_checked(t, p, "*2\r\n+hello\r\n+world\r\n"));
   expected_nodes = {
      {type::simple_string, 1u, 0u, "message"},
      {type::array,         2u, 0u, ""       },
      {type::simple_string, 1u, 1u, "hello"  },
      {type::simple_string, 1u, 1u, "world"  },
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 17u);
   BOOST_TEST_EQ(t.get_total_msgs(), 2u);
}

// --- Move assignment ---
void test_move_assign()
{
   flat_tree t;
   add_nodes(t, "+some_data\r\n");

   flat_tree t2;
   add_nodes(t2, "*2\r\n+hello\r\n+world\r\n");

   t = std::move(t2);

   std::vector<node_view> expected_nodes{
      {type::array,         2u, 0u, ""     },
      {type::simple_string, 1u, 1u, "hello"},
      {type::simple_string, 1u, 1u, "world"},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 10u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);
   BOOST_TEST_EQ(t.get_reallocs(), 1u);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);
}

// The lhs is empty and doesn't have any capacity
void test_move_assign_target_empty()
{
   flat_tree t;

   flat_tree t2;
   add_nodes(t2, "+hello\r\n");

   t = std::move(t2);

   std::vector<node_view> expected_nodes{
      {type::simple_string, 1u, 0u, "hello"},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 5u);
   BOOST_TEST_EQ(t.data_capacity(), 512u);
   BOOST_TEST_EQ(t.get_reallocs(), 1u);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);
}

// If the source of the assignment is empty, nothing bad happens
void test_move_assign_source_empty()
{
   flat_tree t;
   add_nodes(t, "+hello\r\n");

   flat_tree t2;

   t = std::move(t2);

   check_nodes(t, {});
   BOOST_TEST_EQ(t.data_size(), 0u);
   BOOST_TEST_EQ(t.data_capacity(), 0u);
   BOOST_TEST_EQ(t.get_reallocs(), 0u);
   BOOST_TEST_EQ(t.get_total_msgs(), 0u);
}

// If both source and target are empty, nothing bad happens
void test_move_assign_both_empty()
{
   flat_tree t;

   flat_tree t2;

   t = std::move(t2);

   check_nodes(t, {});
   BOOST_TEST_EQ(t.data_size(), 0u);
   BOOST_TEST_EQ(t.data_capacity(), 0u);
   BOOST_TEST_EQ(t.get_reallocs(), 0u);
   BOOST_TEST_EQ(t.get_total_msgs(), 0u);
}

// Move assignment also propagates the tmp area
void test_move_assign_tmp()
{
   parser p;

   flat_tree t;
   add_nodes(t, "+some_data\r\n");

   flat_tree t2;
   add_nodes(t2, "+message\r\n");
   BOOST_TEST_NOT(parse_checked(t2, p, "*2\r\n+hello\r\n"));

   // When moving, the tmp area is moved, too
   t = std::move(t2);
   std::vector<node_view> expected_nodes{
      {type::simple_string, 1u, 0u, "message"},
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 7u);
   BOOST_TEST_EQ(t.get_total_msgs(), 1u);

   // Finish the message
   BOOST_TEST(parse_checked(t, p, "*2\r\n+hello\r\n+world\r\n"));
   expected_nodes = {
      {type::simple_string, 1u, 0u, "message"},
      {type::array,         2u, 0u, ""       },
      {type::simple_string, 1u, 1u, "hello"  },
      {type::simple_string, 1u, 1u, "world"  },
   };
   check_nodes(t, expected_nodes);
   BOOST_TEST_EQ(t.data_size(), 17u);
   BOOST_TEST_EQ(t.get_total_msgs(), 2u);
}

// --- Iterators ---
// We can obtain iterators using begin() and end() and use them to iterate
void test_iterators()
{
   // Setup
   flat_tree t;
   add_nodes(t, "+node1\r\n");
   add_nodes(t, ":200\r\n");
   constexpr node_view node1{type::simple_string, 1u, 0u, "node1"};
   constexpr node_view node2{type::number, 1u, 0u, "200"};

   // These methods are const
   const auto& tconst = t;
   auto it = tconst.begin();
   auto end = tconst.end();

   // Iteration using iterators
   BOOST_TEST_NE(it, end);
   BOOST_TEST_EQ(*it, node1);
   BOOST_TEST_NE(++it, end);
   BOOST_TEST_EQ(*it, node2);
   BOOST_TEST_EQ(++it, end);

   // Iteration using range for
   std::vector<node_view> nodes;
   for (const auto& n : t)
      nodes.push_back(n);
   constexpr std::array expected_nodes{node1, node2};
   BOOST_TEST_ALL_EQ(nodes.begin(), nodes.end(), expected_nodes.begin(), expected_nodes.end());
}

// Empty ranges don't cause trouble
void test_iterators_empty()
{
   flat_tree t;
   BOOST_TEST_EQ(t.begin(), t.end());
}

// Tmp area is not included in the range
// More or less tested with the add_nodes tests
void test_iterators_tmp()
{
   parser p;
   flat_tree t;
   BOOST_TEST_NOT(parse_checked(t, p, "*2\r\n+hello\r\n"));
   BOOST_TEST_EQ(t.begin(), t.end());
}

// The iterator should be contiguous
#ifdef BOOST_REDIS_TEST_RANGE_CONCEPTS
static_assert(std::contiguous_iterator<flat_tree::iterator>);
#endif

// --- Reverse iterators ---
// We can obtain iterators using rbegin() and rend() and use them to iterate
void test_reverse_iterators()
{
   // Setup
   flat_tree t;
   add_nodes(t, "+node1\r\n");
   add_nodes(t, ":200\r\n");

   // These methods are const
   const auto& tconst = t;

   constexpr node_view expected_nodes[] = {
      {type::number,        1u, 0u, "200"  },
      {type::simple_string, 1u, 0u, "node1"},
   };
   BOOST_TEST_ALL_EQ(
      tconst.rbegin(),
      tconst.rend(),
      std::begin(expected_nodes),
      std::end(expected_nodes));
}

// Empty ranges don't cause trouble
void test_reverse_iterators_empty()
{
   flat_tree t;
   BOOST_TEST(t.rbegin() == t.rend());
}

// Tmp area is not included in the range
void test_reverse_iterators_tmp()
{
   parser p;
   flat_tree t;

   // Add one full message and a partial one
   add_nodes(t, "*1\r\n+node1\r\n");
   BOOST_TEST_NOT(parse_checked(t, p, "*2\r\n+hello\r\n"));

   // Only the full message appears in the reversed range
   constexpr node_view expected_nodes[] = {
      {type::simple_string, 1u, 1u, "node1"},
      {type::array,         1u, 0u, ""     },
   };
   BOOST_TEST_ALL_EQ(t.rbegin(), t.rend(), std::begin(expected_nodes), std::end(expected_nodes));
}

// --- at ---
void test_at()
{
   parser p;
   flat_tree t;

   // Add one full message and a partial one
   add_nodes(t, "*1\r\n+node1\r\n");
   BOOST_TEST_NOT(parse_checked(t, p, "*2\r\n+hello\r\n"));

   // Nodes in the range can be accessed with at()
   constexpr node_view n0{type::array, 1u, 0u, ""};
   constexpr node_view n1{type::simple_string, 1u, 1u, "node1"};
   BOOST_TEST_EQ(t.at(0u), n0);
   BOOST_TEST_EQ(t.at(1u), n1);

   // Nodes in the tmp area are not considered in range
   BOOST_TEST_THROWS(t.at(2u), std::out_of_range);
   BOOST_TEST_THROWS(t.at(3u), std::out_of_range);

   // Indices out of range throw
   BOOST_TEST_THROWS(t.at(4u), std::out_of_range);
   BOOST_TEST_THROWS(t.at(5u), std::out_of_range);
   BOOST_TEST_THROWS(t.at((std::numeric_limits<std::size_t>::max)()), std::out_of_range);
}

// Empty ranges don't cause trouble
void test_at_empty()
{
   flat_tree t;
   BOOST_TEST_THROWS(t.at(0u), std::out_of_range);
   BOOST_TEST_THROWS(t.at(2u), std::out_of_range);
   BOOST_TEST_THROWS(t.at((std::numeric_limits<std::size_t>::max)()), std::out_of_range);
}

// --- operator[], front, back ---
void test_unchecked_access()
{
   flat_tree t;
   add_nodes(t, "*2\r\n+node1\r\n+node2\r\n");

   constexpr node_view n0{type::array, 2u, 0u, ""};
   constexpr node_view n1{type::simple_string, 1u, 1u, "node1"};
   constexpr node_view n2{type::simple_string, 1u, 1u, "node2"};

   // operator []
   BOOST_TEST_EQ(t[0u], n0);
   BOOST_TEST_EQ(t[1u], n1);
   BOOST_TEST_EQ(t[2u], n2);

   // Front and back
   BOOST_TEST_EQ(t.front(), n0);
   BOOST_TEST_EQ(t.back(), n2);
}

// --- data ---
void test_data()
{
   flat_tree t;
   add_nodes(t, "*1\r\n+node1\r\n");

   constexpr node_view expected_nodes[] = {
      {type::array,         1u, 0u, ""     },
      {type::simple_string, 1u, 1u, "node1"},
   };

   BOOST_TEST_NE(t.data(), nullptr);
   BOOST_TEST_ALL_EQ(t.data(), t.data() + 2u, std::begin(expected_nodes), std::end(expected_nodes));
}

// Empty ranges don't cause trouble
void test_data_empty()
{
   flat_tree t;
   BOOST_TEST_EQ(t.data(), nullptr);
}

// --- size and empty ---
void test_size()
{
   flat_tree t;
   add_nodes(t, "*1\r\n+node1\r\n");

   BOOST_TEST_EQ(t.size(), 2u);
   BOOST_TEST_NOT(t.empty());
}

void test_size_empty()
{
   flat_tree t;

   BOOST_TEST_EQ(t.size(), 0u);
   BOOST_TEST(t.empty());
}

// Tmp area not taken into account
void test_size_tmp()
{
   parser p;
   flat_tree t;

   // Add one full message and a partial one
   add_nodes(t, "*1\r\n+node1\r\n");
   BOOST_TEST_NOT(parse_checked(t, p, "*2\r\n+hello\r\n"));

   BOOST_TEST_EQ(t.size(), 2u);
   BOOST_TEST_NOT(t.empty());
}

void test_size_tmp_only()
{
   parser p;
   flat_tree t;

   // Add one partial message
   BOOST_TEST_NOT(parse_checked(t, p, "*2\r\n+hello\r\n"));

   BOOST_TEST_EQ(t.size(), 0u);
   BOOST_TEST(t.empty());
}

// The range should model contiguous range
#ifdef BOOST_REDIS_TEST_RANGE_CONCEPTS
static_assert(std::ranges::contiguous_range<flat_tree>);
#endif

// --- Comparison ---
void test_comparison_different()
{
   flat_tree t;
   add_nodes(t, "+some_data\r\n");

   flat_tree t2;
   add_nodes(t2, "*2\r\n+hello\r\n+world\r\n");

   BOOST_TEST_NOT(t == t2);
   BOOST_TEST(t != t2);
   BOOST_TEST_NOT(t2 == t);
   BOOST_TEST(t2 != t);
}

// The only difference is node types
void test_comparison_different_node_types()
{
   flat_tree t;
   add_nodes(t, "+hello\r\n");

   flat_tree t2;
   add_nodes(t2, "$5\r\nhello\r\n");

   BOOST_TEST_NOT(t == t2);
   BOOST_TEST(t != t2);
}

void test_comparison_equal()
{
   flat_tree t;
   add_nodes(t, "+some_data\r\n");

   flat_tree t2;
   add_nodes(t2, "+some_data\r\n");

   BOOST_TEST(t == t2);
   BOOST_TEST_NOT(t != t2);
}

// Allocations are not taken into account when comparing
void test_comparison_equal_reallocations()
{
   const std::string big_node(2000u, 'a');
   flat_tree t;
   t.reserve(100u, 5u);
   add_nodes(t, "+" + big_node + "\r\n");
   BOOST_TEST_EQ(t.get_reallocs(), 2u);

   flat_tree t2;
   t2.reserve(2048u, 5u);
   add_nodes(t2, "+" + big_node + "\r\n");
   BOOST_TEST_EQ(t2.get_reallocs(), 1u);

   BOOST_TEST(t == t2);
   BOOST_TEST_NOT(t != t2);
}

// Capacity is not taken into account when comparing
void test_comparison_equal_capacity()
{
   flat_tree t;
   add_nodes(t, "+hello\r\n");

   flat_tree t2;
   t2.reserve(2048u, 5u);
   add_nodes(t2, "+hello\r\n");

   BOOST_TEST(t == t2);
   BOOST_TEST_NOT(t != t2);
}

// Empty containers don't cause trouble
void test_comparison_empty()
{
   flat_tree t;
   add_nodes(t, "$5\r\nhello\r\n");

   flat_tree tempty, tempty2;

   BOOST_TEST_NOT(t == tempty);
   BOOST_TEST(t != tempty);

   BOOST_TEST_NOT(tempty == t);
   BOOST_TEST(tempty != t);

   BOOST_TEST(tempty == tempty2);
   BOOST_TEST_NOT(tempty != tempty2);
}

// Self comparisons don't cause trouble
void test_comparison_self()
{
   flat_tree t;
   add_nodes(t, "$5\r\nhello\r\n");

   flat_tree tempty;

   BOOST_TEST(t == t);
   BOOST_TEST_NOT(t != t);

   BOOST_TEST(tempty == tempty);
   BOOST_TEST_NOT(tempty != tempty);
}

// The tmp area is not taken into account when comparing
void test_comparison_tmp()
{
   flat_tree t;
   add_nodes(t, "+hello\r\n");

   flat_tree t2;
   add_nodes(t2, "+hello\r\n");
   parser p;
   BOOST_TEST_NOT(parse_checked(t2, p, "*2\r\n+more data\r\n"));

   BOOST_TEST(t == t2);
   BOOST_TEST_NOT(t != t2);
}

void test_comparison_tmp_different()
{
   flat_tree t;
   add_nodes(t, "+hello\r\n");

   flat_tree t2;
   add_nodes(t2, "+world\r\n");
   parser p;
   BOOST_TEST_NOT(parse_checked(t2, p, "*2\r\n+more data\r\n"));

   BOOST_TEST_NOT(t == t2);
   BOOST_TEST(t != t2);
}

// Comparing object with only tmp area doesn't cause trouble
void test_comparison_only_tmp()
{
   flat_tree t;
   parser p;
   BOOST_TEST_NOT(parse_checked(t, p, "*2\r\n+more data\r\n"));

   flat_tree t2;
   parser p2;
   BOOST_TEST_NOT(parse_checked(t2, p2, "*2\r\n+random\r\n"));

   BOOST_TEST(t == t2);
   BOOST_TEST_NOT(t != t2);
}

// --- Capacity ---
// Delegates to the underlying vector function
void test_capacity()
{
   flat_tree t;
   BOOST_TEST_EQ(t.capacity(), 0u);

   // Inserting a node increases capacity.
   // It is not specified how capacity grows, though.
   add_nodes(t, "+hello\r\n");
   BOOST_TEST_GE(t.capacity(), 1u);

   // Reserve also affects capacity
   t.reserve(1000u, 8u);
   BOOST_TEST_GE(t.capacity(), 8u);
}

}  // namespace

int main()
{
   test_add_nodes();
   test_add_nodes_copies();
   test_add_nodes_capacity_limit();
   test_add_nodes_big_node();
   test_add_nodes_tmp();
   test_add_nodes_existing_tmp();
   test_add_nodes_existing_data_and_tmp();

   test_reserve();
   test_reserve_not_power_of_2();
   test_reserve_below_current_capacity();
   test_reserve_with_data();
   test_reserve_with_tmp();

   test_clear();
   test_clear_empty();
   test_clear_reuse();
   test_clear_tmp();
   test_clear_only_tmp();
   test_clear_only_tmp_nodes();

   test_default_constructor();

   test_copy_ctor();
   test_copy_ctor_empty();
   test_copy_ctor_empty_with_capacity();
   test_copy_ctor_adjust_capacity();
   test_copy_ctor_tmp();

   test_move_ctor();
   test_move_ctor_empty();
   test_move_ctor_with_capacity();
   test_move_ctor_tmp();

   test_copy_assign();
   test_copy_assign_target_empty();
   test_copy_assign_target_not_enough_capacity();
   test_copy_assign_source_empty();
   test_copy_assign_source_with_capacity();
   test_copy_assign_source_with_extra_capacity();
   test_copy_assign_both_empty();
   test_copy_assign_self();
   test_copy_assign_tmp();

   test_move_assign();
   test_move_assign_target_empty();
   test_move_assign_source_empty();
   test_move_assign_both_empty();
   test_move_assign_tmp();

   test_iterators();
   test_iterators_empty();
   test_iterators_tmp();

   test_reverse_iterators();
   test_reverse_iterators_empty();
   test_reverse_iterators_tmp();

   test_at();
   test_at_empty();

   test_unchecked_access();

   test_data();
   test_data_empty();

   test_size();
   test_size_empty();
   test_size_tmp();
   test_size_tmp_only();

   test_comparison_different();
   test_comparison_different_node_types();
   test_comparison_equal();
   test_comparison_equal_reallocations();
   test_comparison_equal_capacity();
   test_comparison_empty();
   test_comparison_self();
   test_comparison_tmp();
   test_comparison_tmp_different();
   test_comparison_only_tmp();

   test_capacity();

   return boost::report_errors();
}

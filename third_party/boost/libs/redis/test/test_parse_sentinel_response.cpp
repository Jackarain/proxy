//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/adapter/any_adapter.hpp>
#include <boost/redis/config.hpp>
#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/impl/sentinel_utils.hpp>
#include <boost/redis/resp3/node.hpp>
#include <boost/redis/resp3/parser.hpp>

#include <boost/assert/source_location.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/system/error_code.hpp>

#include "sansio_utils.hpp"

#include <initializer_list>
#include <ostream>
#include <string_view>
#include <vector>

using namespace boost::redis;
using detail::tree_from_resp3;
using detail::parse_sentinel_response;
using detail::sentinel_response;
using boost::system::error_code;

// Operators
namespace boost::redis {

std::ostream& operator<<(std::ostream& os, const address& addr)
{
   return os << "address{ .host=" << addr.host << ", .port=" << addr.port << " }";
}

}  // namespace boost::redis

namespace {

struct fixture {
   sentinel_response resp{
      "leftover",
      {"leftover_host", "6543"},
      {address()},
      {address()},
   };

   void check_response(
      const address& expected_master_addr,
      boost::span<const address> expected_replicas,
      boost::span<const address> expected_sentinels,
      boost::source_location loc = BOOST_CURRENT_LOCATION) const
   {
      if (!BOOST_TEST_EQ(resp.diagnostic, ""))
         std::cerr << "Called from " << loc << std::endl;
      if (!BOOST_TEST_EQ(resp.master_addr, expected_master_addr))
         std::cerr << "Called from " << loc << std::endl;
      if (!BOOST_TEST_ALL_EQ(
             resp.replicas.begin(),
             resp.replicas.end(),
             expected_replicas.begin(),
             expected_replicas.end()))
         std::cerr << "Called from " << loc << std::endl;
      if (!BOOST_TEST_ALL_EQ(
             resp.sentinels.begin(),
             resp.sentinels.end(),
             expected_sentinels.begin(),
             expected_sentinels.end()))
         std::cerr << "Called from " << loc << std::endl;
   }
};

// Usual response when asking for a master
void test_master()
{
   // Setup
   fixture fix;
   auto nodes = tree_from_resp3({
      // clang-format off
      "*2\r\n$9\r\nlocalhost\r\n$4\r\n6380\r\n",
      "*2\r\n"
         "%14\r\n"
            "$4\r\nname\r\n$40\r\nf14ef06a8a478cdd66ded467ec18accd2a24b731\r\n$2\r\nip\r\n$8\r\nhost.one\r\n$4\r\nport\r\n$5\r\n26380\r\n"
            "$5\r\nrunid\r\n$40\r\nf14ef06a8a478cdd66ded467ec18accd2a24b731\r\n$5\r\nflags\r\n$8\r\nsentinel\r\n"
            "$21\r\nlink-pending-commands\r\n$1\r\n0\r\n$13\r\nlink-refcount\r\n$1\r\n1\r\n$14\r\nlast-ping-sent\r\n$1\r\n0\r\n"
            "$18\r\nlast-ok-ping-reply\r\n$3\r\n696\r\n$15\r\nlast-ping-reply\r\n$3\r\n696\r\n$23\r\ndown-after-milliseconds\r\n$5\r\n10000\r\n"
            "$18\r\nlast-hello-message\r\n$3\r\n334\r\n$12\r\nvoted-leader\r\n$1\r\n?\r\n$18\r\nvoted-leader-epoch\r\n$1\r\n0\r\n"
         "%14\r\n"
            "$4\r\nname\r\n$40\r\nf9b54e79e2e7d3f17ad60527504191ec8a861f27\r\n$2\r\nip\r\n$8\r\nhost.two\r\n$4\r\nport\r\n$5\r\n26381\r\n"
            "$5\r\nrunid\r\n$40\r\nf9b54e79e2e7d3f17ad60527504191ec8a861f27\r\n$5\r\nflags\r\n$8\r\nsentinel\r\n"
            "$21\r\nlink-pending-commands\r\n$1\r\n0\r\n$13\r\nlink-refcount\r\n$1\r\n1\r\n$14\r\nlast-ping-sent\r\n$1\r\n0\r\n"
            "$18\r\nlast-ok-ping-reply\r\n$3\r\n696\r\n$15\r\nlast-ping-reply\r\n$3\r\n696\r\n$23\r\ndown-after-milliseconds\r\n$5\r\n10000\r\n"
            "$18\r\nlast-hello-message\r\n$3\r\n134\r\n$12\r\nvoted-leader\r\n$1\r\n?\r\n$18\r\nvoted-leader-epoch\r\n$1\r\n0\r\n",
      // clang-format on
   });

   // Call the function
   auto ec = parse_sentinel_response(nodes, role::master, fix.resp);
   BOOST_TEST_EQ(ec, error_code());

   // Check
   const address expected_sentinels[] = {
      {"host.one", "26380"},
      {"host.two", "26381"},
   };
   fix.check_response({"localhost", "6380"}, {}, expected_sentinels);
}

// Works correctly even if no Sentinels are present
void test_master_no_sentinels()
{
   // Setup
   fixture fix;
   auto nodes = tree_from_resp3({
      "*2\r\n$9\r\nlocalhost\r\n$4\r\n6380\r\n",
      "*0\r\n",
   });

   // Call the function
   auto ec = parse_sentinel_response(nodes, role::master, fix.resp);
   BOOST_TEST_EQ(ec, error_code());
   fix.check_response({"localhost", "6380"}, {}, {});
}

// The responses corresponding to the user-defined setup request are ignored
void test_master_setup_request()
{
   // Setup
   fixture fix;
   auto nodes = tree_from_resp3({
      // clang-format off
      "+OK\r\n",
      "%6\r\n$6\r\nserver\r\n$5\r\nredis\r\n$7\r\nversion\r\n$5\r\n7.4.2\r\n$5\r\nproto\r\n:3\r\n$2\r\nid\r\n:3\r\n$4\r\nmode\r\n$8\r\nsentinel\r\n$7\r\nmodules\r\n*0\r\n",
      "*2\r\n$9\r\nlocalhost\r\n$4\r\n6380\r\n",
      "*2\r\n"
         "%14\r\n"
            "$4\r\nname\r\n$40\r\nf14ef06a8a478cdd66ded467ec18accd2a24b731\r\n$2\r\nip\r\n$8\r\nhost.one\r\n$4\r\nport\r\n$5\r\n26380\r\n"
            "$5\r\nrunid\r\n$40\r\nf14ef06a8a478cdd66ded467ec18accd2a24b731\r\n$5\r\nflags\r\n$8\r\nsentinel\r\n"
            "$21\r\nlink-pending-commands\r\n$1\r\n0\r\n$13\r\nlink-refcount\r\n$1\r\n1\r\n$14\r\nlast-ping-sent\r\n$1\r\n0\r\n"
            "$18\r\nlast-ok-ping-reply\r\n$3\r\n696\r\n$15\r\nlast-ping-reply\r\n$3\r\n696\r\n$23\r\ndown-after-milliseconds\r\n$5\r\n10000\r\n"
            "$18\r\nlast-hello-message\r\n$3\r\n334\r\n$12\r\nvoted-leader\r\n$1\r\n?\r\n$18\r\nvoted-leader-epoch\r\n$1\r\n0\r\n"
         "%14\r\n"
            "$4\r\nname\r\n$40\r\nf9b54e79e2e7d3f17ad60527504191ec8a861f27\r\n$2\r\nip\r\n$8\r\nhost.two\r\n$4\r\nport\r\n$5\r\n26381\r\n"
            "$5\r\nrunid\r\n$40\r\nf9b54e79e2e7d3f17ad60527504191ec8a861f27\r\n$5\r\nflags\r\n$8\r\nsentinel\r\n"
            "$21\r\nlink-pending-commands\r\n$1\r\n0\r\n$13\r\nlink-refcount\r\n$1\r\n1\r\n$14\r\nlast-ping-sent\r\n$1\r\n0\r\n"
            "$18\r\nlast-ok-ping-reply\r\n$3\r\n696\r\n$15\r\nlast-ping-reply\r\n$3\r\n696\r\n$23\r\ndown-after-milliseconds\r\n$5\r\n10000\r\n"
            "$18\r\nlast-hello-message\r\n$3\r\n134\r\n$12\r\nvoted-leader\r\n$1\r\n?\r\n$18\r\nvoted-leader-epoch\r\n$1\r\n0\r\n",
      // clang-format on
   });

   // Call the function
   auto ec = parse_sentinel_response(nodes, role::master, fix.resp);
   BOOST_TEST_EQ(ec, error_code());

   // Check
   const address expected_sentinels[] = {
      {"host.one", "26380"},
      {"host.two", "26381"},
   };
   fix.check_response({"localhost", "6380"}, {}, expected_sentinels);
}

// IP and port can be out of order
void test_master_ip_port_out_of_order()
{
   // Setup
   fixture fix;
   auto nodes = tree_from_resp3({
      // clang-format off
      "*2\r\n$9\r\nlocalhost\r\n$4\r\n6380\r\n",
      "*1\r\n"
         "%2\r\n"
            "$4\r\nport\r\n$5\r\n26380\r\n$2\r\nip\r\n$8\r\nhost.one\r\n"
      // clang-format on
   });

   // Call the function
   auto ec = parse_sentinel_response(nodes, role::master, fix.resp);
   BOOST_TEST_EQ(ec, error_code());

   // Check
   const address expected_sentinels[] = {
      {"host.one", "26380"},
   };
   fix.check_response({"localhost", "6380"}, {}, expected_sentinels);
}

// Usual response when asking for a replica
void test_replica()
{
   // Setup
   fixture fix;
   auto nodes = tree_from_resp3({
      // clang-format off
      "*2\r\n$9\r\nlocalhost\r\n$4\r\n6380\r\n",
      "*2\r\n"
         "%21\r\n"
            "$4\r\nname\r\n$14\r\nlocalhost:6381\r\n$2\r\nip\r\n$9\r\nsome.host\r\n$4\r\nport\r\n$4\r\n6381\r\n"
            "$5\r\nrunid\r\n$40\r\ncdfa33e2d39958c0b10c0391c0c3d4ab096edfeb\r\n$5\r\nflags\r\n$5\r\nslave\r\n"
            "$21\r\nlink-pending-commands\r\n$1\r\n0\r\n$13\r\nlink-refcount\r\n$1\r\n1\r\n$14\r\nlast-ping-sent\r\n$1\r\n0\r\n"
            "$18\r\nlast-ok-ping-reply\r\n$3\r\n134\r\n$15\r\nlast-ping-reply\r\n$3\r\n134\r\n$23\r\ndown-after-milliseconds\r\n$5\r\n10000\r\n"
            "$12\r\ninfo-refresh\r\n$4\r\n5302\r\n$13\r\nrole-reported\r\n$5\r\nslave\r\n$18\r\nrole-reported-time\r\n$6\r\n442121\r\n"
            "$21\r\nmaster-link-down-time\r\n$1\r\n0\r\n$18\r\nmaster-link-status\r\n$2\r\nok\r\n$11\r\nmaster-host\r\n$9\r\nlocalhost\r\n"
            "$11\r\nmaster-port\r\n$4\r\n6380\r\n$14\r\nslave-priority\r\n$3\r\n100\r\n$17\r\nslave-repl-offset\r\n$5\r\n29110\r\n"
            "$17\r\nreplica-announced\r\n$1\r\n1\r\n"
         "%21\r\n"
            "$4\r\nname\r\n$14\r\nlocalhost:6382\r\n$2\r\nip\r\n$9\r\ntest.host\r\n$4\r\nport\r\n$4\r\n6382\r\n"
            "$5\r\nrunid\r\n$40\r\n11bfea62c25316e211fdf0e1ccd2dbd920e90815\r\n$5\r\nflags\r\n$5\r\nslave\r\n"
            "$21\r\nlink-pending-commands\r\n$1\r\n0\r\n$13\r\nlink-refcount\r\n$1\r\n1\r\n$14\r\nlast-ping-sent\r\n$1\r\n0\r\n"
            "$18\r\nlast-ok-ping-reply\r\n$3\r\n134\r\n$15\r\nlast-ping-reply\r\n$3\r\n134\r\n$23\r\ndown-after-milliseconds\r\n$5\r\n10000\r\n"
            "$12\r\ninfo-refresh\r\n$4\r\n5302\r\n$13\r\nrole-reported\r\n$5\r\nslave\r\n$18\r\nrole-reported-time\r\n$6\r\n442132\r\n"
            "$21\r\nmaster-link-down-time\r\n$1\r\n0\r\n$18\r\nmaster-link-status\r\n$2\r\nok\r\n$11\r\nmaster-host\r\n$9\r\nlocalhost\r\n"
            "$11\r\nmaster-port\r\n$4\r\n6380\r\n$14\r\nslave-priority\r\n$3\r\n100\r\n$17\r\nslave-repl-offset\r\n$5\r\n29110\r\n"
            "$17\r\nreplica-announced\r\n$1\r\n1\r\n",
      "*2\r\n"
         "%14\r\n"
            "$4\r\nname\r\n$40\r\nf14ef06a8a478cdd66ded467ec18accd2a24b731\r\n$2\r\nip\r\n$8\r\nhost.one\r\n$4\r\nport\r\n$5\r\n26380\r\n"
            "$5\r\nrunid\r\n$40\r\nf14ef06a8a478cdd66ded467ec18accd2a24b731\r\n$5\r\nflags\r\n$8\r\nsentinel\r\n"
            "$21\r\nlink-pending-commands\r\n$1\r\n0\r\n$13\r\nlink-refcount\r\n$1\r\n1\r\n$14\r\nlast-ping-sent\r\n$1\r\n0\r\n"
            "$18\r\nlast-ok-ping-reply\r\n$3\r\n696\r\n$15\r\nlast-ping-reply\r\n$3\r\n696\r\n$23\r\ndown-after-milliseconds\r\n$5\r\n10000\r\n"
            "$18\r\nlast-hello-message\r\n$3\r\n334\r\n$12\r\nvoted-leader\r\n$1\r\n?\r\n$18\r\nvoted-leader-epoch\r\n$1\r\n0\r\n"
         "%14\r\n"
            "$4\r\nname\r\n$40\r\nf9b54e79e2e7d3f17ad60527504191ec8a861f27\r\n$2\r\nip\r\n$8\r\nhost.two\r\n$4\r\nport\r\n$5\r\n26381\r\n"
            "$5\r\nrunid\r\n$40\r\nf9b54e79e2e7d3f17ad60527504191ec8a861f27\r\n$5\r\nflags\r\n$8\r\nsentinel\r\n"
            "$21\r\nlink-pending-commands\r\n$1\r\n0\r\n$13\r\nlink-refcount\r\n$1\r\n1\r\n$14\r\nlast-ping-sent\r\n$1\r\n0\r\n"
            "$18\r\nlast-ok-ping-reply\r\n$3\r\n696\r\n$15\r\nlast-ping-reply\r\n$3\r\n696\r\n$23\r\ndown-after-milliseconds\r\n$5\r\n10000\r\n"
            "$18\r\nlast-hello-message\r\n$3\r\n134\r\n$12\r\nvoted-leader\r\n$1\r\n?\r\n$18\r\nvoted-leader-epoch\r\n$1\r\n0\r\n",
      // clang-format on
   });

   // Call the function
   auto ec = parse_sentinel_response(nodes, role::replica, fix.resp);
   BOOST_TEST_EQ(ec, error_code());

   // Check
   const address expected_replicas[] = {
      {"some.host", "6381"},
      {"test.host", "6382"},
   };
   const address expected_sentinels[] = {
      {"host.one", "26380"},
      {"host.two", "26381"},
   };
   fix.check_response({"localhost", "6380"}, expected_replicas, expected_sentinels);
}

// Like the master case
void test_replica_no_sentinels()
{
   // Setup
   fixture fix;
   auto nodes = tree_from_resp3({
      // clang-format off
      "*2\r\n$9\r\nlocalhost\r\n$4\r\n6380\r\n",
      "*2\r\n"
         "%3\r\n"
            "$4\r\nname\r\n$14\r\nlocalhost:6381\r\n$2\r\nip\r\n$9\r\nsome.host\r\n$4\r\nport\r\n$4\r\n6381\r\n"
         "%3\r\n"
            "$4\r\nname\r\n$14\r\nlocalhost:6382\r\n$2\r\nip\r\n$9\r\ntest.host\r\n$4\r\nport\r\n$4\r\n6382\r\n",
      "*0\r\n"
      // clang-format on
   });

   // Call the function
   auto ec = parse_sentinel_response(nodes, role::replica, fix.resp);
   BOOST_TEST_EQ(ec, error_code());

   // Check
   const address expected_replicas[] = {
      {"some.host", "6381"},
      {"test.host", "6382"},
   };
   fix.check_response({"localhost", "6380"}, expected_replicas, {});
}

// Asking for replicas, but there is none
void test_replica_no_replicas()
{
   // Setup
   fixture fix;
   auto nodes = tree_from_resp3({
      // clang-format off
      "*2\r\n$9\r\nlocalhost\r\n$4\r\n6380\r\n",
      "*0\r\n",
      "*0\r\n",
      // clang-format on
   });

   // Call the function
   auto ec = parse_sentinel_response(nodes, role::replica, fix.resp);
   BOOST_TEST_EQ(ec, error_code());

   // Check
   fix.check_response({"localhost", "6380"}, {}, {});
}

// Setup requests work with replicas, too
void test_replica_setup_request()
{
   // Setup
   fixture fix;
   auto nodes = tree_from_resp3({
      // clang-format off
      "*2\r\n+OK\r\n+OK\r\n",
      "*2\r\n$9\r\nlocalhost\r\n$4\r\n6380\r\n",
      "*2\r\n"
         "%3\r\n"
            "$4\r\nname\r\n$14\r\nlocalhost:6381\r\n$2\r\nip\r\n$9\r\nsome.host\r\n$4\r\nport\r\n$4\r\n6381\r\n"
         "%3\r\n"
            "$4\r\nname\r\n$14\r\nlocalhost:6382\r\n$2\r\nip\r\n$9\r\ntest.host\r\n$4\r\nport\r\n$4\r\n6382\r\n",
      "*2\r\n"
         "%3\r\n"
            "$4\r\nname\r\n$40\r\nf14ef06a8a478cdd66ded467ec18accd2a24b731\r\n$2\r\nip\r\n$8\r\nhost.one\r\n$4\r\nport\r\n$5\r\n26380\r\n"
         "%3\r\n"
            "$4\r\nname\r\n$40\r\nf9b54e79e2e7d3f17ad60527504191ec8a861f27\r\n$2\r\nip\r\n$8\r\nhost.two\r\n$4\r\nport\r\n$5\r\n26381\r\n"
      // clang-format on
   });

   // Call the function
   auto ec = parse_sentinel_response(nodes, role::replica, fix.resp);
   BOOST_TEST_EQ(ec, error_code());

   // Check
   const address expected_replicas[] = {
      {"some.host", "6381"},
      {"test.host", "6382"},
   };
   const address expected_sentinels[] = {
      {"host.one", "26380"},
      {"host.two", "26381"},
   };
   fix.check_response({"localhost", "6380"}, expected_replicas, expected_sentinels);
}

// IP and port can be out of order
void test_replica_ip_port_out_of_order()
{
   // Setup
   fixture fix;
   auto nodes = tree_from_resp3({
      // clang-format off
      "*2\r\n$9\r\ntest.host\r\n$4\r\n6389\r\n",
      "*1\r\n"
         "%2\r\n"
            "$4\r\nport\r\n$4\r\n6381\r\n$2\r\nip\r\n$9\r\nsome.host\r\n",
      "*0\r\n"
      // clang-format on
   });

   // Call the function
   auto ec = parse_sentinel_response(nodes, role::replica, fix.resp);
   BOOST_TEST_EQ(ec, error_code());

   // Check
   const address expected_replicas[] = {
      {"some.host", "6381"},
   };
   fix.check_response({"test.host", "6389"}, expected_replicas, {});
}

void test_errors()
{
   const struct {
      std::string_view name;
      role server_role;
      std::vector<std::string_view> responses;
      std::string_view expected_diagnostic;
      error_code expected_ec;
   } test_cases[]{
      // clang-format off
      {
         // A RESP3 simple error
         "setup_error_simple",
         role::master,
         {
            "-WRONGPASS invalid username-password pair or user is disabled.\r\n",
            "*2\r\n$9\r\nlocalhost\r\n$4\r\n6380\r\n",
            "*0\r\n",
         },
         "WRONGPASS invalid username-password pair or user is disabled.",
         error::resp3_simple_error
      },
      {
         // A RESP3 blob error
         "setup_error_blob",
         role::master,
         {
            "!3\r\nBad\r\n",
            "*2\r\n$9\r\nlocalhost\r\n$4\r\n6380\r\n",
            "*0\r\n",
         },
         "Bad",
         error::resp3_blob_error
      },
      {
         // Errors in intermediate nodes of the user-supplied request
         "setup_error_intermediate",
         role::master,
         {
            "+OK\r\n",
            "-Something happened!\r\n",
            "+OK\r\n",
            "*2\r\n$9\r\nlocalhost\r\n$4\r\n6380\r\n",
            "*0\r\n",
         },
         "Something happened!",
         error::resp3_simple_error
      },
      {
         // Only the first error is processed (e.g. auth failure may cause subsequent cmds to fail)
         "setup_error_intermediate",
         role::master,
         {
            "-Something happened!\r\n",
            "-Something worse happened!\r\n",
            "-Bad\r\n",
            "-Worse\r\n",
         },
         "Something happened!",
         error::resp3_simple_error
      },
      {
         // This works for replicas, too
         "setup_error_replicas",
         role::replica,
         {
            "-Something happened!\r\n",
            "-Something worse happened!\r\n",
            "-Bad\r\n",
            "-Worse\r\n",
         },
         "Something happened!",
         error::resp3_simple_error
      },

      // SENTINEL GET-MASTER-ADDR-BY-NAME
      {
         // Unknown master. This returns NULL and causes SENTINEL SENTINELS to fail
         "getmasteraddr_unknown_master",
         role::master,
         {
            "_\r\n",
            "-ERR Unknown master\r\n",
         },
         "",
         error::resp3_null
      },
      {
         // The request errors for any other reason
         "getmasteraddr_error",
         role::master,
         {
            "-ERR something happened\r\n",
            "*0\r\n",
         },
         "ERR something happened",
         error::resp3_simple_error
      },
      {
         // Same, for replicas
         "getmasteraddr_unknown_master_replica",
         role::replica,
         {
            "_\r\n",
            "-ERR Unknown master\r\n",
            "-ERR Unknown master\r\n",
         },
         "",
         error::resp3_null
      },
      {
         // Root node should be a list
         "getmasteraddr_not_array",
         role::master,
         {
            "+OK\r\n",
            "*0\r\n",
         },
         "",
         error::expects_resp3_array
      },
      {
         // Root node should have exactly 2 elements
         "getmasteraddr_array_size_1",
         role::master,
         {
            "*1\r\n$5\r\nhello\r\n",
            "*0\r\n",
         },
         "",
         error::incompatible_size
      },
      {
         // Root node should have exactly 2 elements
         "getmasteraddr_array_size_3",
         role::master,
         {
            "*3\r\n$5\r\nhello\r\n$3\r\nbye\r\n$3\r\nabc\r\n",
            "*0\r\n",
         },
         "",
         error::incompatible_size
      },
      {
         // IP should be a string
         "getmasteraddr_ip_not_string",
         role::master,
         {
            "*2\r\n+OK\r\n$5\r\nhello\r\n",
            "*0\r\n",
         },
         "",
         error::expects_resp3_string
      },
      {
         // Port should be a string
         "getmasteraddr_port_not_string",
         role::master,
         {
            "*2\r\n$5\r\nhello\r\n+OK\r\n",
            "*0\r\n",
         },
         "",
         error::expects_resp3_string
      },

      // SENTINEL SENTINELS
      {
         // The request errors
         "sentinels_error",
         role::master,
         {
            "*2\r\n$9\r\nlocalhost\r\n$4\r\n6380\r\n",
            "-ERR something went wrong\r\n",
         },
         "ERR something went wrong",
         error::resp3_simple_error
      },
      {
         // The root node should be an array
         "sentinels_not_array",
         role::master,
         {
            "*2\r\n$9\r\nlocalhost\r\n$4\r\n6380\r\n",
            "+OK\r\n",
         },
         "",
         error::expects_resp3_array
      },
      {
         // Each Sentinel object should be a map
         "sentinels_subobject_not_map",
         role::master,
         {
            "*2\r\n$9\r\nlocalhost\r\n$4\r\n6380\r\n",
            "*1\r\n*1\r\n$9\r\nlocalhost\r\n",
         },
         "",
         error::expects_resp3_map
      },
      {
         // Keys in the Sentinel object should be strings
         "sentinels_keys_not_strings",
         role::master,
         {
            "*2\r\n$9\r\nlocalhost\r\n$4\r\n6380\r\n",
            "*1\r\n%1\r\n*0\r\n$5\r\nhello\r\n",
         },
         "",
         error::expects_resp3_string
      },
      {
         // Values in the Sentinel object should be strings
         "sentinels_keys_not_strings",
         role::master,
         {
            "*2\r\n$9\r\nlocalhost\r\n$4\r\n6380\r\n",
            "*1\r\n%1\r\n$5\r\nhello\r\n*1\r\n+OK\r\n",
         },
         "",
         error::expects_resp3_string
      },
      {
         "sentinels_ip_not_found",
         role::master,
         {
            "*2\r\n$9\r\nlocalhost\r\n$4\r\n6380\r\n",
            "*1\r\n%1\r\n$4\r\nport\r\n$4\r\n6380\r\n",
         },
         "",
         error::empty_field
      },
      {
         "sentinels_port_not_found",
         role::master,
         {
            "*2\r\n$9\r\nlocalhost\r\n$4\r\n6380\r\n",
            "*1\r\n%1\r\n$2\r\nip\r\n$9\r\nlocalhost\r\n",
         },
         "",
         error::empty_field
      },

      // SENTINEL REPLICAS
      {
         // The request errors
         "replicas_error",
         role::replica,
         {
            "*2\r\n$9\r\nlocalhost\r\n$4\r\n6380\r\n",
            "-ERR something went wrong\r\n",
            "*0\r\n",
         },
         "ERR something went wrong",
         error::resp3_simple_error
      },
      {
         // The root node should be an array
         "replicas_not_array",
         role::replica,
         {
            "*2\r\n$9\r\nlocalhost\r\n$4\r\n6380\r\n",
            "+OK\r\n",
            "*0\r\n",
         },
         "",
         error::expects_resp3_array
      },
      {
         // Each replica object should be a map
         "replicas_subobject_not_map",
         role::replica,
         {
            "*2\r\n$9\r\nlocalhost\r\n$4\r\n6380\r\n",
            "*1\r\n*1\r\n$9\r\nlocalhost\r\n",
            "*0\r\n",
         },
         "",
         error::expects_resp3_map
      },
      {
         // Keys in the replica object should be strings
         "replicas_keys_not_strings",
         role::replica,
         {
            "*2\r\n$9\r\nlocalhost\r\n$4\r\n6380\r\n",
            "*1\r\n%1\r\n*0\r\n$5\r\nhello\r\n",
            "*0\r\n",
         },
         "",
         error::expects_resp3_string
      },
      {
         // Values in the replica object should be strings
         "replicas_keys_not_strings",
         role::replica,
         {
            "*2\r\n$9\r\nlocalhost\r\n$4\r\n6380\r\n",
            "*1\r\n%1\r\n$5\r\nhello\r\n*1\r\n+OK\r\n",
            "*0\r\n",
         },
         "",
         error::expects_resp3_string
      },
      {
         "replicas_ip_not_found",
         role::replica,
         {
            "*2\r\n$9\r\nlocalhost\r\n$4\r\n6380\r\n",
            "*1\r\n%1\r\n$4\r\nport\r\n$4\r\n6380\r\n",
            "*0\r\n",
         },
         "",
         error::empty_field
      },
      {
         "replicas_port_not_found",
         role::replica,
         {
            "*2\r\n$9\r\nlocalhost\r\n$4\r\n6380\r\n",
            "*1\r\n%1\r\n$2\r\nip\r\n$9\r\nlocalhost\r\n",
            "*0\r\n",
         },
         "",
         error::empty_field
      },
      {
         "not_enough_messages_1",
         role::master,
         {},
         "",
         error::incompatible_size
      },
      {
         "not_enough_messages_2",
         role::master,
         {
            "*2\r\n$9\r\nlocalhost\r\n$4\r\n6380\r\n",
         },
         "",
         error::incompatible_size
      },
      {
         "not_enough_messages_replica_1",
         role::replica,
         {},
         "",
         error::incompatible_size
      },
      {
         "not_enough_messages_replica_2",
         role::replica,
         {
            "*2\r\n$9\r\nlocalhost\r\n$4\r\n6380\r\n",
            "*0\r\n",
         },
         "",
         error::incompatible_size
      },

      // clang-format on
   };

   for (const auto& tc : test_cases) {
      // Setup
      std::cerr << "Running error test case: " << tc.name << std::endl;
      fixture fix;
      auto nodes = tree_from_resp3(tc.responses);

      // Call the function
      auto ec = parse_sentinel_response(nodes, tc.server_role, fix.resp);
      BOOST_TEST_EQ(ec, tc.expected_ec);
      BOOST_TEST_EQ(fix.resp.diagnostic, tc.expected_diagnostic);
   }
}

}  // namespace

int main()
{
   test_master();
   test_master_no_sentinels();
   test_master_setup_request();
   test_master_ip_port_out_of_order();

   test_replica();
   test_replica_no_sentinels();
   test_replica_no_replicas();
   test_replica_setup_request();
   test_replica_ip_port_out_of_order();

   test_errors();

   return boost::report_errors();
}

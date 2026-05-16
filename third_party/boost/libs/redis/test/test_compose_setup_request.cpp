//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/config.hpp>
#include <boost/redis/detail/subscription_tracker.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/impl/setup_request_utils.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/resp3/type.hpp>

#include <boost/asio/error.hpp>
#include <boost/assert/source_location.hpp>
#include <boost/core/lightweight_test.hpp>

#include <iostream>
#include <string_view>

using namespace boost::redis;
namespace asio = boost::asio;
using detail::compose_setup_request;
using detail::subscription_tracker;
using boost::system::error_code;

namespace {

struct fixture {
   subscription_tracker tracker;
   request out;
   config cfg;

   void run(std::string_view expected_payload, boost::source_location loc = BOOST_CURRENT_LOCATION)
   {
      out.push("PING", "leftover");  // verify that we clear the request

      compose_setup_request(cfg, tracker, out);

      if (!BOOST_TEST_EQ(out.payload(), expected_payload))
         std::cerr << "Called from " << loc << std::endl;

      if (!BOOST_TEST(out.has_hello_priority()))
         std::cerr << "Called from " << loc << std::endl;

      if (!BOOST_TEST(out.get_config().cancel_if_unresponded))
         std::cerr << "Called from " << loc << std::endl;

      if (!BOOST_TEST(out.get_config().cancel_on_connection_lost))
         std::cerr << "Called from " << loc << std::endl;
   }
};

void test_hello()
{
   fixture fix;
   fix.cfg.clientname = "";

   fix.run("*2\r\n$5\r\nHELLO\r\n$1\r\n3\r\n");
}

void test_select()
{
   fixture fix;
   fix.cfg.clientname = "";
   fix.cfg.database_index = 10;

   fix.run(
      "*2\r\n$5\r\nHELLO\r\n$1\r\n3\r\n"
      "*2\r\n$6\r\nSELECT\r\n$2\r\n10\r\n");
}

void test_clientname()
{
   fixture fix;

   fix.run("*4\r\n$5\r\nHELLO\r\n$1\r\n3\r\n$7\r\nSETNAME\r\n$11\r\nBoost.Redis\r\n");
}

void test_auth()
{
   fixture fix;
   fix.cfg.clientname = "";
   fix.cfg.username = "foo";
   fix.cfg.password = "bar";

   fix.run("*5\r\n$5\r\nHELLO\r\n$1\r\n3\r\n$4\r\nAUTH\r\n$3\r\nfoo\r\n$3\r\nbar\r\n");
}

void test_auth_empty_password()
{
   fixture fix;
   fix.cfg.clientname = "";
   fix.cfg.username = "foo";

   fix.run("*5\r\n$5\r\nHELLO\r\n$1\r\n3\r\n$4\r\nAUTH\r\n$3\r\nfoo\r\n$0\r\n\r\n");
}

void test_auth_setname()
{
   fixture fix;
   fix.cfg.clientname = "mytest";
   fix.cfg.username = "foo";
   fix.cfg.password = "bar";

   fix.run(
      "*7\r\n$5\r\nHELLO\r\n$1\r\n3\r\n$4\r\nAUTH\r\n$3\r\nfoo\r\n$3\r\nbar\r\n$7\r\nSETNAME\r\n$"
      "6\r\nmytest\r\n");
}

void test_use_setup()
{
   fixture fix;
   fix.cfg.clientname = "mytest";
   fix.cfg.username = "foo";
   fix.cfg.password = "bar";
   fix.cfg.database_index = 4;
   fix.cfg.use_setup = true;
   fix.cfg.setup.push("SELECT", 8);

   fix.run(
      "*2\r\n$5\r\nHELLO\r\n$1\r\n3\r\n"
      "*2\r\n$6\r\nSELECT\r\n$1\r\n8\r\n");
}

// Regression check: we set the priority flag
void test_use_setup_no_hello()
{
   fixture fix;
   fix.cfg.use_setup = true;
   fix.cfg.setup.clear();
   fix.cfg.setup.push("SELECT", 8);

   fix.run("*2\r\n$6\r\nSELECT\r\n$1\r\n8\r\n");
}

// Regression check: we set the relevant cancellation flags in the request
void test_use_setup_flags()
{
   fixture fix;
   fix.cfg.use_setup = true;
   fix.cfg.setup.clear();
   fix.cfg.setup.push("SELECT", 8);
   fix.cfg.setup.get_config().cancel_if_unresponded = false;
   fix.cfg.setup.get_config().cancel_on_connection_lost = false;

   fix.run("*2\r\n$6\r\nSELECT\r\n$1\r\n8\r\n");
}

// If we have tracked subscriptions, these are added at the end
void test_tracked_subscriptions()
{
   fixture fix;
   fix.cfg.clientname = "";

   // Populate the tracker
   request sub_req;
   sub_req.subscribe({"ch1", "ch2"});
   fix.tracker.commit_changes(sub_req);

   fix.run(
      "*2\r\n$5\r\nHELLO\r\n$1\r\n3\r\n"
      "*3\r\n$9\r\nSUBSCRIBE\r\n$3\r\nch1\r\n$3\r\nch2\r\n");
}

void test_tracked_subscriptions_use_setup()
{
   fixture fix;
   fix.cfg.use_setup = true;
   fix.cfg.setup.clear();
   fix.cfg.setup.push("PING", "value");

   // Populate the tracker
   request sub_req;
   sub_req.subscribe({"ch1", "ch2"});
   fix.tracker.commit_changes(sub_req);

   fix.run(
      "*2\r\n$4\r\nPING\r\n$5\r\nvalue\r\n"
      "*3\r\n$9\r\nSUBSCRIBE\r\n$3\r\nch1\r\n$3\r\nch2\r\n");
}

// When using Sentinel, a ROLE command is added. This works
// both with the old HELLO and new setup strategies, and with tracked subscriptions
void test_sentinel_auth()
{
   fixture fix;
   fix.cfg.sentinel.addresses = {
      {"localhost", "26379"}
   };
   fix.cfg.clientname = "";
   fix.cfg.username = "foo";
   fix.cfg.password = "bar";

   fix.run(
      "*5\r\n$5\r\nHELLO\r\n$1\r\n3\r\n$4\r\nAUTH\r\n$3\r\nfoo\r\n$3\r\nbar\r\n"
      "*1\r\n$4\r\nROLE\r\n");
}

void test_sentinel_use_setup()
{
   fixture fix;
   fix.cfg.sentinel.addresses = {
      {"localhost", "26379"}
   };
   fix.cfg.use_setup = true;
   fix.cfg.setup.push("SELECT", 42);

   fix.run(
      "*2\r\n$5\r\nHELLO\r\n$1\r\n3\r\n"
      "*2\r\n$6\r\nSELECT\r\n$2\r\n42\r\n"
      "*1\r\n$4\r\nROLE\r\n");
}

void test_sentinel_tracked_subscriptions()
{
   fixture fix;
   fix.cfg.clientname = "";
   fix.cfg.sentinel.addresses = {
      {"localhost", "26379"}
   };

   // Populate the tracker
   request sub_req;
   sub_req.subscribe({"ch1", "ch2"});
   fix.tracker.commit_changes(sub_req);

   fix.run(
      "*2\r\n$5\r\nHELLO\r\n$1\r\n3\r\n"
      "*1\r\n$4\r\nROLE\r\n"
      "*3\r\n$9\r\nSUBSCRIBE\r\n$3\r\nch1\r\n$3\r\nch2\r\n");
}

}  // namespace

int main()
{
   test_hello();
   test_select();
   test_clientname();
   test_auth();
   test_auth_empty_password();
   test_auth_setname();
   test_use_setup();
   test_use_setup_no_hello();
   test_use_setup_flags();
   test_tracked_subscriptions();
   test_tracked_subscriptions_use_setup();
   test_sentinel_auth();
   test_sentinel_use_setup();
   test_sentinel_tracked_subscriptions();

   return boost::report_errors();
}
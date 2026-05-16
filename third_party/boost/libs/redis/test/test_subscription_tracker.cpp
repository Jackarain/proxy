//
// Copyright (c) 2025-2026 Marcelo Zimbres Silva (mzimbres@gmail.com),
// Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/detail/subscription_tracker.hpp>
#include <boost/redis/request.hpp>

#include <boost/core/lightweight_test.hpp>

using namespace boost::redis;
using detail::subscription_tracker;

namespace {

// State originated by SUBSCRIBE commands, only
void test_subscribe()
{
   subscription_tracker tracker;
   request req1, req2, req_output, req_expected;

   // Add some changes to the tracker
   req1.subscribe({"channel_a", "channel_b"});
   tracker.commit_changes(req1);

   req2.subscribe({"channel_c"});
   tracker.commit_changes(req2);

   // Check that we generate the correct response
   tracker.compose_subscribe_request(req_output);
   req_expected.push("SUBSCRIBE", "channel_a", "channel_b", "channel_c");
   BOOST_TEST_EQ(req_output.payload(), req_expected.payload());
}

// State originated by PSUBSCRIBE commands, only
void test_psubscribe()
{
   subscription_tracker tracker;
   request req1, req2, req_output, req_expected;

   // Add some changes to the tracker
   req1.psubscribe({"channel_b*", "channel_c*"});
   tracker.commit_changes(req1);

   req2.psubscribe({"channel_a*"});
   tracker.commit_changes(req2);

   // Check that we generate the correct response
   tracker.compose_subscribe_request(req_output);
   req_expected.push("PSUBSCRIBE", "channel_a*", "channel_b*", "channel_c*");  // we sort them
   BOOST_TEST_EQ(req_output.payload(), req_expected.payload());
}

// We can mix SUBSCRIBE and PSUBSCRIBE operations
void test_subscribe_psubscribe()
{
   subscription_tracker tracker;
   request req1, req2, req_output, req_expected;

   // Add some changes to the tracker
   req1.psubscribe({"channel_a*", "channel_b*"});
   req1.subscribe({"ch1"});
   tracker.commit_changes(req1);

   req2.subscribe({"ch2"});
   req2.psubscribe({"channel_c*"});
   tracker.commit_changes(req2);

   // Check that we generate the correct response
   tracker.compose_subscribe_request(req_output);
   req_expected.push("SUBSCRIBE", "ch1", "ch2");
   req_expected.push("PSUBSCRIBE", "channel_a*", "channel_b*", "channel_c*");
   BOOST_TEST_EQ(req_output.payload(), req_expected.payload());
}

// We can have subscribe and psubscribe commands with the same argument
void test_subscribe_psubscribe_same_arg()
{
   subscription_tracker tracker;
   request req, req_output, req_expected;

   req.subscribe({"ch1"});
   req.psubscribe({"ch1"});
   tracker.commit_changes(req);

   tracker.compose_subscribe_request(req_output);
   req_expected.push("SUBSCRIBE", "ch1");
   req_expected.push("PSUBSCRIBE", "ch1");
   BOOST_TEST_EQ(req_output.payload(), req_expected.payload());
}

// An unsubscribe/punsubscribe balances a matching subscribe
void test_unsubscribe()
{
   subscription_tracker tracker;
   request req1, req2, req_output, req_expected;

   // Add some changes to the tracker
   req1.subscribe({"ch1", "ch2"});
   req1.psubscribe({"ch1*", "ch2*"});
   tracker.commit_changes(req1);

   // Unsubscribe from some channels
   req2.punsubscribe({"ch2*"});
   req2.unsubscribe({"ch1"});
   tracker.commit_changes(req2);

   // Check that we generate the correct response
   tracker.compose_subscribe_request(req_output);
   req_expected.push("SUBSCRIBE", "ch2");
   req_expected.push("PSUBSCRIBE", "ch1*");
   BOOST_TEST_EQ(req_output.payload(), req_expected.payload());
}

// After an unsubscribe, we can subscribe again
void test_resubscribe()
{
   subscription_tracker tracker;
   request req, req_output, req_expected;

   // Subscribe to some channels
   req.subscribe({"ch1", "ch2"});
   req.psubscribe({"ch1*", "ch2*"});
   tracker.commit_changes(req);

   // Unsubscribe from some channels
   req.clear();
   req.punsubscribe({"ch2*"});
   req.unsubscribe({"ch1"});
   tracker.commit_changes(req);

   // Subscribe again
   req.clear();
   req.subscribe({"ch1"});
   req.psubscribe({"ch2*"});
   tracker.commit_changes(req);

   // Check that we generate the correct response
   tracker.compose_subscribe_request(req_output);
   req_expected.push("SUBSCRIBE", "ch1", "ch2");
   req_expected.push("PSUBSCRIBE", "ch1*", "ch2*");
   BOOST_TEST_EQ(req_output.payload(), req_expected.payload());
}

// Subscribing twice is not a problem
void test_subscribe_twice()
{
   subscription_tracker tracker;
   request req, req_output, req_expected;

   // Subscribe to some channels
   req.subscribe({"ch1", "ch2"});
   req.psubscribe({"ch1*", "ch2*"});
   tracker.commit_changes(req);

   // Subscribe to the same channels again
   req.clear();
   req.subscribe({"ch2"});
   req.psubscribe({"ch1*"});
   tracker.commit_changes(req);

   // Check that we generate the correct response
   tracker.compose_subscribe_request(req_output);
   req_expected.push("SUBSCRIBE", "ch1", "ch2");
   req_expected.push("PSUBSCRIBE", "ch1*", "ch2*");
   BOOST_TEST_EQ(req_output.payload(), req_expected.payload());
}

// Unsubscribing from channels we haven't subscribed to is not a problem
void test_lone_unsubscribe()
{
   subscription_tracker tracker;
   request req, req_output, req_expected;

   // Subscribe to some channels
   req.subscribe({"ch1", "ch2"});
   req.psubscribe({"ch1*", "ch2*"});
   tracker.commit_changes(req);

   // Unsubscribe from channels we haven't subscribed to
   req.clear();
   req.unsubscribe({"other"});
   req.punsubscribe({"other*"});
   tracker.commit_changes(req);

   // Check that we generate the correct response
   tracker.compose_subscribe_request(req_output);
   req_expected.push("SUBSCRIBE", "ch1", "ch2");
   req_expected.push("PSUBSCRIBE", "ch1*", "ch2*");
   BOOST_TEST_EQ(req_output.payload(), req_expected.payload());
}

// A state with no changes is not a problem
void test_empty()
{
   subscription_tracker tracker;
   request req_output;

   tracker.compose_subscribe_request(req_output);
   BOOST_TEST_EQ(req_output.payload(), "");
}

// If the output request is not empty, the commands are added to it, rather than replaced
void test_output_request_not_empty()
{
   subscription_tracker tracker;
   request req, req_output, req_expected;

   // Subscribe to some channels
   req.subscribe({"ch1", "ch2"});
   req.psubscribe({"ch1*", "ch2*"});
   tracker.commit_changes(req);

   // Compose the output request
   req_output.push("PING", "hello");
   tracker.compose_subscribe_request(req_output);

   // Check that we generate the correct response
   req_expected.push("PING", "hello");
   req_expected.push("SUBSCRIBE", "ch1", "ch2");
   req_expected.push("PSUBSCRIBE", "ch1*", "ch2*");
   BOOST_TEST_EQ(req_output.payload(), req_expected.payload());
}

// Clear removes everything from the state
void test_clear()
{
   subscription_tracker tracker;
   request req, req_output, req_expected;

   // Subscribe to some channels
   req.subscribe({"ch1", "ch2"});
   req.psubscribe({"ch1*", "ch2*"});
   tracker.commit_changes(req);

   // Clear
   tracker.clear();

   // Nothing should be generated
   tracker.compose_subscribe_request(req_output);
   BOOST_TEST_EQ(req_output.payload(), "");

   // We can reuse the tracker by now committing some more changes
   req.clear();
   req.subscribe({"ch5"});
   req.psubscribe({"ch6*"});
   tracker.commit_changes(req);

   // Check that we generate the correct response
   tracker.compose_subscribe_request(req_output);
   req_expected.push("SUBSCRIBE", "ch5");
   req_expected.push("PSUBSCRIBE", "ch6*");
   BOOST_TEST_EQ(req_output.payload(), req_expected.payload());
}

}  // namespace

int main()
{
   test_subscribe();
   test_psubscribe();
   test_subscribe_psubscribe();
   test_subscribe_psubscribe_same_arg();
   test_unsubscribe();
   test_resubscribe();
   test_subscribe_twice();
   test_lone_unsubscribe();
   test_empty();
   test_output_request_not_empty();
   test_clear();

   return boost::report_errors();
}

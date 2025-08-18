//
// Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/redis/detail/reader_fsm.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/system/error_code.hpp>

namespace net = boost::asio;
namespace redis = boost::redis;
using boost::system::error_code;
using net::cancellation_type_t;
using redis::detail::reader_fsm;
using redis::detail::multiplexer;
using redis::generic_response;
using action = redis::detail::reader_fsm::action;

namespace boost::redis::detail {

extern auto to_string(reader_fsm::action::type t) noexcept -> char const*;

std::ostream& operator<<(std::ostream& os, reader_fsm::action::type t)
{
   os << to_string(t);
   return os;
}
}  // namespace boost::redis::detail

// Operators
namespace {

void test_push()
{
   multiplexer mpx;
   generic_response resp;
   mpx.set_receive_response(resp);
   reader_fsm fsm{mpx};
   error_code ec;
   action act;

   // Initiate
   act = fsm.resume(0, ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::setup_cancellation);
   act = fsm.resume(0, ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::append_some);

   // The fsm is asking for data.
   mpx.get_read_buffer().append(">1\r\n+msg1\r\n");
   mpx.get_read_buffer().append(">1\r\n+msg2 \r\n");
   mpx.get_read_buffer().append(">1\r\n+msg3  \r\n");
   auto const bytes_read = mpx.get_read_buffer().size();

   // Deliver the 1st push
   act = fsm.resume(bytes_read, ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::notify_push_receiver);
   BOOST_TEST_EQ(act.push_size_, 11u);
   BOOST_TEST_EQ(act.ec_, error_code());

   // Deliver the 2st push
   act = fsm.resume(0, ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::notify_push_receiver);
   BOOST_TEST_EQ(act.push_size_, 12u);
   BOOST_TEST_EQ(act.ec_, error_code());

   // Deliver the 3rd push
   act = fsm.resume(0, ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::notify_push_receiver);
   BOOST_TEST_EQ(act.push_size_, 13u);
   BOOST_TEST_EQ(act.ec_, error_code());

   // All pushes were delivered so the fsm should demand more data
   act = fsm.resume(0, ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::append_some);
   BOOST_TEST_EQ(act.ec_, error_code());
}

void test_read_needs_more()
{
   multiplexer mpx;
   generic_response resp;
   mpx.set_receive_response(resp);
   reader_fsm fsm{mpx};
   error_code ec;
   action act;

   // Initiate
   act = fsm.resume(0, ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::setup_cancellation);
   act = fsm.resume(0, ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::append_some);

   // Split the incoming message in three random parts and deliver
   // them to the reader individually.
   std::string const msg[] = {">3\r", "\n+msg1\r\n+ms", "g2\r\n+msg3\r\n"};

   // Passes the first part to the fsm.
   mpx.get_read_buffer().append(msg[0]);
   act = fsm.resume(msg[0].size(), ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::needs_more);
   BOOST_TEST_EQ(act.ec_, error_code());

   // Passes the second part to the fsm.
   mpx.get_read_buffer().append(msg[1]);
   act = fsm.resume(msg[1].size(), ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::needs_more);
   BOOST_TEST_EQ(act.ec_, error_code());

   // Passes the third and last part to the fsm, next it should ask us
   // to deliver the message.
   mpx.get_read_buffer().append(msg[2]);
   act = fsm.resume(msg[2].size(), ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::notify_push_receiver);
   BOOST_TEST_EQ(act.push_size_, msg[0].size() + msg[1].size() + msg[2].size());
   BOOST_TEST_EQ(act.ec_, error_code());

   // All pushes were delivered so the fsm should demand more data
   act = fsm.resume(0, ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::append_some);
   BOOST_TEST_EQ(act.ec_, error_code());
}

void test_read_error()
{
   multiplexer mpx;
   generic_response resp;
   mpx.set_receive_response(resp);
   reader_fsm fsm{mpx};
   error_code ec;
   action act;

   // Initiate
   act = fsm.resume(0, ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::setup_cancellation);
   act = fsm.resume(0, ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::append_some);

   // The fsm is asking for data.
   mpx.get_read_buffer().append(">1\r\n+msg1\r\n");
   auto const bytes_read = mpx.get_read_buffer().size();

   // Deliver the data
   act = fsm.resume(bytes_read, {net::error::operation_aborted}, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::cancel_run);
   BOOST_TEST_EQ(act.ec_, error_code());

   // Finish
   act = fsm.resume(bytes_read, ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::done);
   BOOST_TEST_EQ(act.ec_, error_code{net::error::operation_aborted});
}

void test_parse_error()
{
   multiplexer mpx;
   generic_response resp;
   mpx.set_receive_response(resp);
   reader_fsm fsm{mpx};
   error_code ec;
   action act;

   // Initiate
   act = fsm.resume(0, ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::setup_cancellation);
   act = fsm.resume(0, ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::append_some);

   // The fsm is asking for data.
   mpx.get_read_buffer().append(">a\r\n");
   auto const bytes_read = mpx.get_read_buffer().size();

   // Deliver the data
   act = fsm.resume(bytes_read, {}, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::cancel_run);
   BOOST_TEST_EQ(act.ec_, error_code());

   // Finish
   act = fsm.resume(bytes_read, {}, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::done);
   BOOST_TEST_EQ(act.ec_, error_code{redis::error::not_a_number});
}

void test_push_deliver_error()
{
   multiplexer mpx;
   generic_response resp;
   mpx.set_receive_response(resp);
   reader_fsm fsm{mpx};
   error_code ec;
   action act;

   // Initiate
   act = fsm.resume(0, ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::setup_cancellation);
   act = fsm.resume(0, ec, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::append_some);

   // The fsm is asking for data.
   mpx.get_read_buffer().append(">1\r\n+msg1\r\n");
   auto const bytes_read = mpx.get_read_buffer().size();

   // Deliver the data
   act = fsm.resume(bytes_read, {}, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::notify_push_receiver);
   BOOST_TEST_EQ(act.ec_, error_code());

   // Resumes from notifying a push with an error.
   act = fsm.resume(bytes_read, net::error::operation_aborted, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::cancel_run);

   // Finish
   act = fsm.resume(0, {}, cancellation_type_t::none);
   BOOST_TEST_EQ(act.type_, action::type::done);
   BOOST_TEST_EQ(act.ec_, error_code{net::error::operation_aborted});
}

}  // namespace

int main()
{
   test_push_deliver_error();
   test_read_needs_more();
   test_push();
   test_read_error();
   test_parse_error();

   return boost::report_errors();
}

/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_READER_FSM_HPP
#define BOOST_REDIS_READER_FSM_HPP

#include <boost/redis/detail/multiplexer.hpp>

#include <boost/asio/cancellation_type.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>

namespace boost::redis::detail {

class reader_fsm {
public:
   struct action {
      enum class type
      {
         setup_cancellation,
         append_some,
         needs_more,
         notify_push_receiver,
         cancel_run,
         done,
      };

      type type_ = type::setup_cancellation;
      std::size_t push_size_ = 0;
      system::error_code ec_ = {};
   };

   explicit reader_fsm(multiplexer& mpx) noexcept;

   action resume(
      std::size_t bytes_read,
      system::error_code ec,
      asio::cancellation_type_t /*cancel_state*/);

private:
   int resume_point_{0};
   action action_after_resume_;
   action::type next_read_type_ = action::type::append_some;
   multiplexer* mpx_ = nullptr;
   std::pair<tribool, std::size_t> res_{std::make_pair(std::nullopt, 0)};
};

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_READER_FSM_HPP

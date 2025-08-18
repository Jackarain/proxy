/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/detail/coroutine.hpp>
#include <boost/redis/detail/multiplexer.hpp>
#include <boost/redis/detail/reader_fsm.hpp>

namespace boost::redis::detail {

reader_fsm::reader_fsm(multiplexer& mpx) noexcept
: mpx_{&mpx}
{ }

reader_fsm::action reader_fsm::resume(
   std::size_t bytes_read,
   system::error_code ec,
   asio::cancellation_type_t /*cancel_state*/)
{
   switch (resume_point_) {
      BOOST_REDIS_CORO_INITIAL
      BOOST_REDIS_YIELD(resume_point_, 1, action::type::setup_cancellation)

      for (;;) {
         BOOST_REDIS_YIELD(resume_point_, 2, next_read_type_)
         if (ec) {
            // TODO: If an error occurred but data was read (i.e.
            // bytes_read != 0) we should try to process that data and
            // deliver it to the user before calling cancel_run.
            action_after_resume_ = {action::type::done, bytes_read, ec};
            BOOST_REDIS_YIELD(resume_point_, 3, action::type::cancel_run)
            return action_after_resume_;
         }

         next_read_type_ = action::type::append_some;
         while (!mpx_->get_read_buffer().empty()) {
            res_ = mpx_->consume_next(ec);
            if (ec) {
               action_after_resume_ = {action::type::done, res_.second, ec};
               BOOST_REDIS_YIELD(resume_point_, 4, action::type::cancel_run)
               return action_after_resume_;
            }

            if (!res_.first.has_value()) {
               next_read_type_ = action::type::needs_more;
               break;
            }

            if (res_.first.value()) {
               BOOST_REDIS_YIELD(resume_point_, 6, action::type::notify_push_receiver, res_.second)
               if (ec) {
                  action_after_resume_ = {action::type::done, 0u, ec};
                  BOOST_REDIS_YIELD(resume_point_, 7, action::type::cancel_run)
                  return action_after_resume_;
               }
            } else {
               // TODO: Here we should notify the exec operation that
               // it can be completed. This will improve log clarity
               // and will make this code symmetrical in how it
               // handles pushes and other messages. The new action
               // type can be named notify_exec. To do that we need to
               // refactor the multiplexer.
            }
         }
      }
   }

   BOOST_ASSERT(false);
   return {action::type::done, 0, system::error_code()};
}

}  // namespace boost::redis::detail

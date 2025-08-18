/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/detail/multiplexer.hpp>
#include <boost/redis/request.hpp>

#include <memory>

namespace boost::redis::detail {

multiplexer::elem::elem(request const& req, pipeline_adapter_type adapter)
: req_{&req}
, adapter_{}
, remaining_responses_{req.get_expected_responses()}
, status_{status::waiting}
, ec_{}
, read_size_{0}
{
   adapter_ = [this, adapter](resp3::node_view const& nd, system::error_code& ec) {
      auto const i = req_->get_expected_responses() - remaining_responses_;
      adapter(i, nd, ec);
   };
}

auto multiplexer::elem::notify_error(system::error_code ec) noexcept -> void
{
   if (!ec_) {
      ec_ = ec;
   }

   notify_done();
}

auto multiplexer::elem::commit_response(std::size_t read_size) -> void
{
   read_size_ += read_size;
   --remaining_responses_;
}

bool multiplexer::remove(std::shared_ptr<elem> const& ptr)
{
   if (ptr->is_waiting()) {
      reqs_.erase(std::remove(std::begin(reqs_), std::end(reqs_), ptr));
      return true;
   }

   return false;
}

std::size_t multiplexer::commit_write()
{
   // We have to clear the payload right after writing it to use it
   // as a flag that informs there is no ongoing write.
   write_buffer_.clear();

   // There is small optimization possible here: traverse only the
   // partition of unwritten requests instead of them all.
   std::for_each(std::begin(reqs_), std::end(reqs_), [](auto const& ptr) {
      BOOST_ASSERT_MSG(ptr != nullptr, "Expects non-null pointer.");
      if (ptr->is_staged()) {
         ptr->mark_written();
      }
   });

   return release_push_requests();
}

void multiplexer::add(std::shared_ptr<elem> const& info)
{
   reqs_.push_back(info);

   if (info->get_request().has_hello_priority()) {
      auto rend = std::partition_point(std::rbegin(reqs_), std::rend(reqs_), [](auto const& e) {
         return e->is_waiting();
      });

      std::rotate(std::rbegin(reqs_), std::rbegin(reqs_) + 1, rend);
   }
}

std::pair<tribool, std::size_t> multiplexer::consume_next(system::error_code& ec)
{
   // We arrive here in two states:
   //
   //    1. While we are parsing a message. In this case we
   //       don't want to determine the type of the message in the
   //       buffer (i.e. response vs push) but leave it untouched
   //       until the parsing of a complete message ends.
   //
   //    2. On a new message, in which case we have to determine
   //       whether the next messag is a push or a response.
   //
   if (!on_push_)  // Prepare for new message.
      on_push_ = is_next_push();

   if (on_push_) {
      if (!resp3::parse(parser_, read_buffer_, receive_adapter_, ec))
         return std::make_pair(std::nullopt, 0);

      if (ec)
         return std::make_pair(std::make_optional(true), 0);

      auto const size = on_finish_parsing(true);
      return std::make_pair(std::make_optional(true), size);
   }

   BOOST_ASSERT_MSG(
      is_waiting_response(),
      "Not waiting for a response (using MONITOR command perhaps?)");
   BOOST_ASSERT(!reqs_.empty());
   BOOST_ASSERT(reqs_.front() != nullptr);
   BOOST_ASSERT(reqs_.front()->get_remaining_responses() != 0);

   if (!resp3::parse(parser_, read_buffer_, reqs_.front()->get_adapter(), ec))
      return std::make_pair(std::nullopt, 0);

   if (ec) {
      reqs_.front()->notify_error(ec);
      reqs_.pop_front();
      return std::make_pair(std::make_optional(false), 0);
   }

   reqs_.front()->commit_response(parser_.get_consumed());
   if (reqs_.front()->get_remaining_responses() == 0) {
      // Done with this request.
      reqs_.front()->notify_done();
      reqs_.pop_front();
   }

   auto const size = on_finish_parsing(false);
   return std::make_pair(std::make_optional(false), size);
}

void multiplexer::reset()
{
   write_buffer_.clear();
   read_buffer_.clear();
   parser_.reset();
   on_push_ = false;
   cancel_run_called_ = false;
}

std::size_t multiplexer::prepare_write()
{
   // Coalesces the requests and marks them staged. After a
   // successful write staged requests will be marked as written.
   auto const point = std::partition_point(
      std::cbegin(reqs_),
      std::cend(reqs_),
      [](auto const& ri) {
         return !ri->is_waiting();
      });

   std::for_each(point, std::cend(reqs_), [this](auto const& ri) {
      // Stage the request.
      write_buffer_ += ri->get_request().payload();
      ri->mark_staged();
      usage_.commands_sent += ri->get_request().get_commands();
   });

   usage_.bytes_sent += std::size(write_buffer_);

   auto const d = std::distance(point, std::cend(reqs_));
   return static_cast<std::size_t>(d);
}

std::size_t multiplexer::cancel_waiting()
{
   auto f = [](auto const& ptr) {
      BOOST_ASSERT(ptr != nullptr);
      return !ptr->is_waiting();
   };

   auto point = std::stable_partition(std::begin(reqs_), std::end(reqs_), f);

   auto const ret = std::distance(point, std::end(reqs_));

   std::for_each(point, std::end(reqs_), [](auto const& ptr) {
      ptr->notify_error({asio::error::operation_aborted});
   });

   reqs_.erase(point, std::end(reqs_));
   return ret;
}

auto multiplexer::cancel_on_conn_lost() -> std::size_t
{
   // Protects the code below from being called more than
   // once, see https://github.com/boostorg/redis/issues/181
   if (std::exchange(cancel_run_called_, true)) {
      return 0;
   }

   // Must return false if the request should be removed.
   auto cond = [](auto const& ptr) {
      BOOST_ASSERT(ptr != nullptr);

      if (ptr->is_waiting()) {
         return !ptr->get_request().get_config().cancel_on_connection_lost;
      } else {
         return !ptr->get_request().get_config().cancel_if_unresponded;
      }
   };

   auto point = std::stable_partition(std::begin(reqs_), std::end(reqs_), cond);

   auto const ret = std::distance(point, std::end(reqs_));

   std::for_each(point, std::end(reqs_), [](auto const& ptr) {
      ptr->notify_error({asio::error::operation_aborted});
   });

   reqs_.erase(point, std::end(reqs_));

   std::for_each(std::begin(reqs_), std::end(reqs_), [](auto const& ptr) {
      return ptr->mark_waiting();
   });

   return ret;
}

std::size_t multiplexer::on_finish_parsing(bool is_push)
{
   if (is_push) {
      usage_.pushes_received += 1;
      usage_.push_bytes_received += parser_.get_consumed();
   } else {
      usage_.responses_received += 1;
      usage_.response_bytes_received += parser_.get_consumed();
   }

   on_push_ = false;
   read_buffer_.erase(0, parser_.get_consumed());
   auto const size = parser_.get_consumed();
   parser_.reset();
   return size;
}

bool multiplexer::is_next_push() const noexcept
{
   BOOST_ASSERT(!read_buffer_.empty());

   // Useful links to understand the heuristics below.
   //
   // - https://github.com/redis/redis/issues/11784
   // - https://github.com/redis/redis/issues/6426
   // - https://github.com/boostorg/redis/issues/170

   // The message's resp3 type is a push.
   if (resp3::to_type(read_buffer_.front()) == resp3::type::push)
      return true;

   // This is non-push type and the requests queue is empty. I have
   // noticed this is possible, for example with -MISCONF. I don't
   // know why they are not sent with a push type so we can
   // distinguish them from responses to commands. If we are lucky
   // enough to receive them when the command queue is empty they
   // can be treated as server pushes, otherwise it is impossible
   // to handle them properly
   if (reqs_.empty())
      return true;

   // The request does not expect any response but we got one. This
   // may happen if for example, subscribe with wrong syntax.
   if (reqs_.front()->get_remaining_responses() == 0)
      return true;

   // Added to deal with MONITOR and also to fix PR170 which
   // happens under load and on low-latency networks, where we
   // might start receiving responses before the write operation
   // completed and the request is still maked as staged and not
   // written.
   return reqs_.front()->is_waiting();
}

std::size_t multiplexer::release_push_requests()
{
   auto point = std::stable_partition(std::begin(reqs_), std::end(reqs_), [](auto const& ptr) {
      return !(ptr->is_written() && ptr->get_request().get_expected_responses() == 0);
   });

   std::for_each(point, std::end(reqs_), [](auto const& ptr) {
      ptr->notify_done();
   });

   auto const d = std::distance(point, std::end(reqs_));
   reqs_.erase(point, std::end(reqs_));
   return static_cast<std::size_t>(d);
}

bool multiplexer::is_waiting_response() const noexcept
{
   if (std::empty(reqs_))
      return false;

   // Under load and on low-latency networks we might start
   // receiving responses before the write operation completed and
   // the request is still maked as staged and not written.  See
   // https://github.com/boostorg/redis/issues/170
   return !reqs_.front()->is_waiting();
}

bool multiplexer::is_writing() const noexcept { return !write_buffer_.empty(); }

auto make_elem(request const& req, multiplexer::pipeline_adapter_type adapter)
   -> std::shared_ptr<multiplexer::elem>
{
   return std::make_shared<multiplexer::elem>(req, std::move(adapter));
}

}  // namespace boost::redis::detail

/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_MULTIPLEXER_HPP
#define BOOST_REDIS_MULTIPLEXER_HPP

#include <boost/redis/adapter/adapt.hpp>
#include <boost/redis/adapter/any_adapter.hpp>
#include <boost/redis/config.hpp>
#include <boost/redis/operation.hpp>
#include <boost/redis/resp3/type.hpp>
#include <boost/redis/usage.hpp>

#include <boost/asio/experimental/channel.hpp>

#include <algorithm>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace boost::redis {

class request;

namespace detail {

using tribool = std::optional<bool>;

struct multiplexer {
   using adapter_type = std::function<void(resp3::node_view const&, system::error_code&)>;
   using pipeline_adapter_type = std::function<
      void(std::size_t, resp3::node_view const&, system::error_code&)>;

   struct elem {
   public:
      explicit elem(request const& req, pipeline_adapter_type adapter);

      void set_done_callback(std::function<void()> f) noexcept { done_ = std::move(f); };

      auto notify_done() noexcept -> void
      {
         status_ = status::done;
         done_();
      }

      auto notify_error(system::error_code ec) noexcept -> void;

      [[nodiscard]]
      auto is_waiting() const noexcept
      {
         return status_ == status::waiting;
      }

      [[nodiscard]]
      auto is_written() const noexcept
      {
         return status_ == status::written;
      }

      [[nodiscard]]
      auto is_staged() const noexcept
      {
         return status_ == status::staged;
      }

      [[nodiscard]]
      bool is_done() const noexcept
      {
         return status_ == status::done;
      }

      void mark_written() noexcept { status_ = status::written; }

      void mark_staged() noexcept { status_ = status::staged; }

      void mark_waiting() noexcept { status_ = status::waiting; }

      auto get_error() const -> system::error_code const& { return ec_; }

      auto get_request() const -> request const& { return *req_; }

      auto get_read_size() const -> std::size_t { return read_size_; }

      auto get_remaining_responses() const -> std::size_t { return remaining_responses_; }

      auto commit_response(std::size_t read_size) -> void;

      auto get_adapter() -> adapter_type& { return adapter_; }

   private:
      enum class status
      {
         waiting,  // the request hasn't been written yet
         staged,   // we've issued the write for this request, but it hasn't finished yet
         written,  // the request has been written successfully
         done,     // the request has completed and the done callback has been invoked
      };

      request const* req_;
      adapter_type adapter_;

      std::function<void()> done_;

      // Contains the number of commands that haven't been read yet.
      std::size_t remaining_responses_;
      status status_;

      system::error_code ec_;
      std::size_t read_size_;
   };

   auto remove(std::shared_ptr<elem> const& ptr) -> bool;

   [[nodiscard]]
   auto prepare_write() -> std::size_t;

   // Returns the number of requests that have been released because
   // they don't have a response e.g. SUBSCRIBE.
   auto commit_write() -> std::size_t;

   // If the tribool contains no value more data is needed, otherwise
   // if the value is true the message consumed is a push.
   [[nodiscard]]
   auto consume_next(system::error_code& ec) -> std::pair<tribool, std::size_t>;

   auto add(std::shared_ptr<elem> const& ptr) -> void;
   auto reset() -> void;

   [[nodiscard]]
   auto const& get_parser() const noexcept
   {
      return parser_;
   }

   //[[nodiscard]]
   auto cancel_waiting() -> std::size_t;

   //[[nodiscard]]
   auto cancel_on_conn_lost() -> std::size_t;

   [[nodiscard]]
   auto get_cancel_run_state() const noexcept -> bool
   {
      return cancel_run_called_;
   }

   [[nodiscard]]
   auto get_write_buffer() noexcept -> std::string_view
   {
      return std::string_view{write_buffer_};
   }

   [[nodiscard]]
   auto get_read_buffer() noexcept -> std::string&
   {
      return read_buffer_;
   }

   [[nodiscard]]
   auto get_read_buffer() const noexcept -> std::string const&
   {
      return read_buffer_;
   }

   // TODO: Change signature to receive an adapter instead of a
   // response.
   template <class Response>
   void set_receive_response(Response& response)
   {
      using namespace boost::redis::adapter;
      auto g = boost_redis_adapt(response);
      receive_adapter_ = adapter::detail::make_adapter_wrapper(g);
   }

   [[nodiscard]]
   auto get_usage() const noexcept -> usage
   {
      return usage_;
   }

   [[nodiscard]]
   auto is_writing() const noexcept -> bool;

private:
   [[nodiscard]]
   auto is_waiting_response() const noexcept -> bool;

   [[nodiscard]]
   auto on_finish_parsing(bool is_push) -> std::size_t;

   [[nodiscard]]
   auto is_next_push() const noexcept -> bool;

   // Releases the number of requests that have been released.
   [[nodiscard]]
   auto release_push_requests() -> std::size_t;

   std::string read_buffer_;
   std::string write_buffer_;
   std::deque<std::shared_ptr<elem>> reqs_;
   resp3::parser parser_{};
   bool on_push_ = false;
   bool cancel_run_called_ = false;
   usage usage_;
   adapter_type receive_adapter_;
};

auto make_elem(request const& req, multiplexer::pipeline_adapter_type adapter)
   -> std::shared_ptr<multiplexer::elem>;

}  // namespace detail
}  // namespace boost::redis

#endif  // BOOST_REDIS_MULTIPLEXER_HPP

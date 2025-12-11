/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_READ_BUFFER_HPP
#define BOOST_REDIS_READ_BUFFER_HPP

#include <boost/core/span.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>
#include <string_view>
#include <utility>
#include <vector>

namespace boost::redis::detail {

class read_buffer {
public:
   using span_type = span<char>;

   struct consume_result {
      std::size_t consumed;
      std::size_t rotated;
   };

   // See config.hpp for the meaning of these parameters.
   struct config {
      std::size_t read_buffer_append_size = 4096u;
      std::size_t max_read_size = static_cast<std::size_t>(-1);
   };

   // Prepare the buffer to receive more data.
   [[nodiscard]]
   auto prepare() -> system::error_code;

   [[nodiscard]]
   auto get_prepared() noexcept -> span_type;

   void commit(std::size_t read_size);

   [[nodiscard]]
   auto get_commited() const noexcept -> std::string_view;

   void clear();

   // Consumes committed data by rotating the remaining data to the
   // front of the buffer.
   auto consume(std::size_t size) -> consume_result;

   void reserve(std::size_t n);

   friend bool operator==(read_buffer const& lhs, read_buffer const& rhs);

   friend bool operator!=(read_buffer const& lhs, read_buffer const& rhs);

   void set_config(config const& cfg) noexcept { cfg_ = cfg; };

private:
   config cfg_ = config{};
   std::vector<char> buffer_;
   std::size_t append_buf_begin_ = 0;
};

}  // namespace boost::redis::detail

#endif  // BOOST_REDIS_READ_BUFFER_HPP

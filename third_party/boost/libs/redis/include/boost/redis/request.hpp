/* Copyright (c) 2018-2024 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_REQUEST_HPP
#define BOOST_REDIS_REQUEST_HPP

#include <boost/redis/resp3/serialization.hpp>
#include <boost/redis/resp3/type.hpp>

#include <algorithm>
#include <string>
#include <tuple>

// NOTE: For some commands like hset it would be a good idea to assert
// the value type is a pair.

namespace boost::redis {

namespace detail {
auto has_response(std::string_view cmd) -> bool;
}

/** @brief Represents a Redis request.
 *  
 *  A request is composed of one or more Redis commands and is
 *  referred to in the redis documentation as a pipeline. See
 *  <a href="https://redis.io/topics/pipelining"></a> for more info.
 *
 *  For example:
 *
 *  @code
 *  request r;
 *  r.push("HELLO", 3);
 *  r.push("FLUSHALL");
 *  r.push("PING");
 *  r.push("PING", "key");
 *  r.push("QUIT");
 *  @endcode
 *
 *  Uses a `std::string` for internal storage.
 */
class request {
public:
   /// Request configuration options.
   struct config {
      /** @brief If `true`, calls to @ref basic_connection::async_exec will
       * complete with error if the connection is lost while the
       * request hasn't been sent yet.
       */
      bool cancel_on_connection_lost = true;

      /** @brief If `true`, @ref basic_connection::async_exec will complete with
       * @ref boost::redis::error::not_connected if the call happens
       * before the connection with Redis was established.
       */
      bool cancel_if_not_connected = false;

      /** @brief If `false`, @ref basic_connection::async_exec will not
       * automatically cancel this request if the connection is lost.
       * Affects only requests that have been written to the socket
       * but have not been responded when
       * @ref boost::redis::connection::async_run completes.
       */
      bool cancel_if_unresponded = true;

      /** @brief If this request has a `HELLO` command and this flag
       * is `true`, it will be moved to the
       * front of the queue of awaiting requests. This makes it
       * possible to send `HELLO` commands and authenticate before other
       * commands are sent.
       */
      bool hello_with_priority = true;
   };

   /** @brief Constructor
    *  
    *  @param cfg Configuration options.
    */
   explicit request(config cfg = config{true, false, true, true})
   : cfg_{cfg}
   { }

   /// Returns the number of responses expected for this request.
   [[nodiscard]] auto get_expected_responses() const noexcept -> std::size_t
   {
      return expected_responses_;
   };

   /// Returns the number of commands contained in this request.
   [[nodiscard]] auto get_commands() const noexcept -> std::size_t { return commands_; };

   [[nodiscard]] auto payload() const noexcept -> std::string_view { return payload_; }

   [[nodiscard]] auto has_hello_priority() const noexcept -> auto const&
   {
      return has_hello_priority_;
   }

   /// Clears the request preserving allocated memory.
   void clear()
   {
      payload_.clear();
      commands_ = 0;
      expected_responses_ = 0;
      has_hello_priority_ = false;
   }

   /// Calls `std::string::reserve` on the internal storage.
   void reserve(std::size_t new_cap = 0) { payload_.reserve(new_cap); }

   /// Returns a reference to the config object.
   [[nodiscard]] auto get_config() const noexcept -> config const& { return cfg_; }

   /// Returns a reference to the config object.
   [[nodiscard]] auto get_config() noexcept -> config& { return cfg_; }

   /** @brief Appends a new command to the end of the request.
    *
    *  For example:
    *
    *  @code
    *  request req;
    *  req.push("SET", "key", "some string", "EX", "2");
    *  @endcode
    *
    *  This will add a `SET` command with value `"some string"` and an
    *  expiration of 2 seconds.
    *
    *  Command arguments should either be convertible to `std::string_view`
    *  or support the `boost_redis_to_bulk` function.
    *  This function is a customization point that must be made available
    *  using ADL and must have the following signature:
    *
    *  @code
    *  void boost_redis_to_bulk(std::string& to, T const& t);
    *  @endcode
    *
    *
    *  See cpp20_serialization.cpp
    *
    *  @param cmd The command to execute. It should be a redis or sentinel command, like `"SET"`.
    *  @param args Command arguments. Non-string types will be converted to string by calling `boost_redis_to_bulk` on each argument.
    *  @tparam Ts Types of the command arguments.
    *
    */
   template <class... Ts>
   void push(std::string_view cmd, Ts const&... args)
   {
      auto constexpr pack_size = sizeof...(Ts);
      resp3::add_header(payload_, resp3::type::array, 1 + pack_size);
      resp3::add_bulk(payload_, cmd);
      resp3::add_bulk(payload_, std::tie(std::forward<Ts const&>(args)...));

      check_cmd(cmd);
   }

   /** @brief Appends a new command to the end of the request.
    *  
    *  This overload is useful for commands that have a key and have a
    *  dynamic range of arguments. For example:
    *
    *  @code
    *  std::map<std::string, std::string> map
    *     { {"key1", "value1"}
    *     , {"key2", "value2"}
    *     , {"key3", "value3"}
    *     };
    *
    *  request req;
    *  req.push_range("HSET", "key", map.cbegin(), map.cend());
    *  @endcode
    *
    *  Command arguments should either be convertible to `std::string_view`
    *  or support the `boost_redis_to_bulk` function.
    *  This function is a customization point that must be made available
    *  using ADL and must have the following signature:
    *
    *  @code
    *  void boost_redis_to_bulk(std::string& to, T const& t);
    *  @endcode
    *  
    *  @param cmd The command to execute. It should be a redis or sentinel command, like `"SET"`.
    *  @param key The command key. It will be added as the first argument to the command.
    *  @param begin Iterator to the begin of the range.
    *  @param end Iterator to the end of the range.
    *  @tparam ForwardIterator A forward iterator with an element type that is convertible to `std::string_view`
    *          or supports `boost_redis_to_bulk`.
    *
    *  See cpp20_serialization.cpp
    */
   template <class ForwardIterator>
   void push_range(
      std::string_view cmd,
      std::string_view key,
      ForwardIterator begin,
      ForwardIterator end,
      typename std::iterator_traits<ForwardIterator>::value_type* = nullptr)
   {
      using value_type = typename std::iterator_traits<ForwardIterator>::value_type;

      if (begin == end)
         return;

      auto constexpr size = resp3::bulk_counter<value_type>::size;
      auto const distance = std::distance(begin, end);
      resp3::add_header(payload_, resp3::type::array, 2 + size * distance);
      resp3::add_bulk(payload_, cmd);
      resp3::add_bulk(payload_, key);

      for (; begin != end; ++begin)
         resp3::add_bulk(payload_, *begin);

      check_cmd(cmd);
   }

   /** @brief Appends a new command to the end of the request.
    *
    *  This overload is useful for commands that have a dynamic number
    *  of arguments and don't have a key. For example:
    *
    *  @code
    *  std::set<std::string> channels
    *     { "channel1" , "channel2" , "channel3" };
    *
    *  request req;
    *  req.push("SUBSCRIBE", std::cbegin(channels), std::cend(channels));
    *  @endcode
    *
    *  Command arguments should either be convertible to `std::string_view`
    *  or support the `boost_redis_to_bulk` function.
    *  This function is a customization point that must be made available
    *  using ADL and must have the following signature:
    *
    *  @code
    *  void boost_redis_to_bulk(std::string& to, T const& t);
    *  @endcode
    *  
    *  @param cmd The command to execute. It should be a redis or sentinel command, like `"SET"`.
    *  @param begin Iterator to the begin of the range.
    *  @param end Iterator to the end of the range.
    *  @tparam ForwardIterator A forward iterator with an element type that is convertible to `std::string_view`
    *          or supports `boost_redis_to_bulk`.
    *
    *  See cpp20_serialization.cpp
    */
   template <class ForwardIterator>
   void push_range(
      std::string_view cmd,
      ForwardIterator begin,
      ForwardIterator end,
      typename std::iterator_traits<ForwardIterator>::value_type* = nullptr)
   {
      using value_type = typename std::iterator_traits<ForwardIterator>::value_type;

      if (begin == end)
         return;

      auto constexpr size = resp3::bulk_counter<value_type>::size;
      auto const distance = std::distance(begin, end);
      resp3::add_header(payload_, resp3::type::array, 1 + size * distance);
      resp3::add_bulk(payload_, cmd);

      for (; begin != end; ++begin)
         resp3::add_bulk(payload_, *begin);

      check_cmd(cmd);
   }

   /** @brief Appends a new command to the end of the request.
    *  
    *  Equivalent to the overload taking a range of begin and end
    *  iterators.
    *  
    *  @param cmd The command to execute. It should be a redis or sentinel command, like `"SET"`.
    *  @param key The command key. It will be added as the first argument to the command.
    *  @param range Range containing the command arguments.
    *  @tparam Range A type that can be passed to `std::begin()` and `std::end()` to obtain
    *          iterators. The range elements should be convertible to `std::string_view`
    *          or support `boost_redis_to_bulk`.
    */
   template <class Range>
   void push_range(
      std::string_view cmd,
      std::string_view key,
      Range const& range,
      decltype(std::begin(range))* = nullptr)
   {
      using std::begin;
      using std::end;
      push_range(cmd, key, begin(range), end(range));
   }

   /** @brief Appends a new command to the end of the request.
    *
    *  Equivalent to the overload taking a range of begin and end
    *  iterators.
    *  
    *  @param cmd The command to execute. It should be a redis or sentinel command, like `"SET"`.
    *  @param range Range containing the command arguments.
    *  @tparam Range A type that can be passed to `std::begin()` and `std::end()` to obtain
    *          iterators. The range elements should be convertible to `std::string_view`
    *          or support `boost_redis_to_bulk`.
    */
   template <class Range>
   void push_range(
      std::string_view cmd,
      Range const& range,
      decltype(std::cbegin(range))* = nullptr)
   {
      using std::cbegin;
      using std::cend;
      push_range(cmd, cbegin(range), cend(range));
   }

private:
   void check_cmd(std::string_view cmd)
   {
      ++commands_;

      if (!detail::has_response(cmd))
         ++expected_responses_;

      if (cmd == "HELLO")
         has_hello_priority_ = cfg_.hello_with_priority;
   }

   config cfg_;
   std::string payload_;
   std::size_t commands_ = 0;
   std::size_t expected_responses_ = 0;
   bool has_hello_priority_ = false;
};

}  // namespace boost::redis

#endif  // BOOST_REDIS_REQUEST_HPP

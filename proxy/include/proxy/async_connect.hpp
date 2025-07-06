//
// async_connect.hpp
// ~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2023_10_18__ASYNC_CONNECT_HPP
#define INCLUDE__2023_10_18__ASYNC_CONNECT_HPP


#include <boost/asio/async_result.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>

#include <boost/smart_ptr/local_shared_ptr.hpp>
#include <boost/smart_ptr/make_local_shared.hpp>
#include <boost/smart_ptr/weak_ptr.hpp>

#include <boost/system/error_code.hpp>

#include <atomic>
#include <iterator>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>


namespace asio_util {
namespace net = boost::asio;

namespace detail {
template <typename Stream, typename Handler> struct connect_context {
  connect_context(Handler &&h) : handler_(std::move(h)) {}

  std::atomic_int flag_;
  std::atomic_int num_;
  Handler handler_;
  std::vector<boost::local_shared_ptr<Stream>> socket_;
};

template <typename Handler, typename ResultType>
void do_result(Handler &&handle, const boost::system::error_code &error,
               ResultType &&result) {
  handle(error, result);
}

template <typename Stream, typename Handler, typename Executor,
          typename Iterator, typename ResultType = void>
void callback(Handler &&handler, Executor ex, Iterator &begin,
              const boost::system::error_code &error) {
  net::post(ex, [error, h = std::move(handler), begin]() mutable {
    if constexpr (std::same_as<ResultType, typename Stream::endpoint_type>)
      do_result(h, error, *begin);

    if constexpr (!std::same_as<ResultType, typename Stream::endpoint_type>)
      do_result(h, error, begin);
  });
}

struct default_connect_condition {
  template <typename Stream, typename Endpoint>
  bool operator()(const boost::system::error_code &, Stream &,
                  const Endpoint &) {
    return true;
  }
};

struct initiate_do_connect {
  bool use_happy_eyeball = false;
  int reject = 0;

  template <typename Stream, typename Endpoint, typename ConnectCondition>
  bool check_condition(const boost::system::error_code &ec, Stream &stream,
                       Endpoint &endp, ConnectCondition connect_condition) {
    bool ret = connect_condition(ec, stream, endp);

    if (!ret)
      reject++;

    return ret;
  }

  template <typename Stream, typename Handler>
  void cancellation_slot(
      boost::local_shared_ptr<connect_context<Stream, Handler>> &context) {
    auto cstate = net::get_associated_cancellation_slot(context->handler_);

    if (!cstate.is_connected())
      return;

    boost::weak_ptr<connect_context<Stream, Handler>> weak_ptr = context;

    cstate.assign([weak_ptr](net::cancellation_type_t) mutable {
      auto context = weak_ptr.lock();
      if (!context)
        return;

      auto &sockets = context->socket_;
      for (auto &stream : sockets) {
        if (!stream)
          continue;

        boost::system::error_code ignore_ec;
        stream->cancel(ignore_ec);
      }
    });
  }

  template <typename Stream, typename Handler, typename Executor,
            typename Iterator, typename ResultType = void>
  bool check_connect_iterator(
      boost::local_shared_ptr<connect_context<Stream, Handler>> &context,
      Executor ex, Iterator begin, Iterator end) {
    context->flag_ = false;
    context->num_ = std::distance(begin, end);

    if (context->num_ == 0) {
      boost::system::error_code error = net::error::not_found;

      callback<Stream, Handler, Executor, Iterator, ResultType>(
          std::move(context->handler_), ex, begin, error);

      return false;
    }

    return true;
  }

  template <typename Iterator>
  void happy_eyeballs_detection(Iterator begin, Iterator end) {
    bool has_a = false, has_aaaa = false;

    for (; begin != end && !(has_a && has_aaaa); begin++) {
      const auto &addr = begin->endpoint().address();

      if (!has_aaaa)
        has_aaaa = addr.is_v6();

      if (!has_a)
        has_a = addr.is_v4();
    }

    if (has_aaaa && has_a)
      use_happy_eyeball = true;
  }

  template <typename Stream, typename Handler, typename Executor,
            typename Iterator, typename ConnectCondition,
            typename ResultType = void>
  void
  do_connect(Iterator iter, Stream &stream,
             boost::local_shared_ptr<connect_context<Stream, Handler>> &context,
             Executor ex, boost::local_shared_ptr<Stream> sock,
             ConnectCondition connect_condition) {
    if (!check_condition({}, *sock, *iter, connect_condition)) {
      if (reject == context->num_) {
        boost::system::error_code error = net::error::not_found;

        callback<Stream, Handler, Executor, Iterator, ResultType>(
            std::forward<Handler>(context->handler_), ex, iter, error);
      }

      return;
    }

    sock->async_connect(
        *iter, [&stream, context, ex, iter,
                sock](const boost::system::error_code &error) mutable {
          if (!error) {
            if (context->flag_.exchange(true))
              return;

            stream = std::move(*sock);
          }

          context->num_--;
          bool is_last = context->num_ == 0;

          if (error) {
            if (context->flag_ || !is_last)
              return;
          }

          auto &sockets = context->socket_;
          for (auto &s : sockets) {
            if (!s)
              continue;

            boost::system::error_code ignore_ec;
            s->cancel(ignore_ec);
          }

          callback<Stream, Handler, Executor, Iterator, ResultType>(
              std::forward<Handler>(context->handler_), ex, iter, error);
        });
  }

  template <typename Stream, typename Handler, typename Iterator,
            typename ConnectCondition, typename ResultType = void>
  void do_async_connect(Handler handler, Stream &stream, Iterator begin,
                        Iterator end, ConnectCondition connect_condition) {
    auto context = boost::make_local_shared<connect_context<Stream, Handler>>(
        std::move(handler));

    // Process handler cancellation slot
    cancellation_slot(context);

    // Get executor from handler or stream.
    auto executor =
        net::get_associated_executor(context->handler_, stream.get_executor());

    // Check connect iterator valid
    if (!check_connect_iterator<Stream, Handler, decltype(executor), Iterator,
                                ResultType>(context, executor, begin, end))
      return;

    // happy eyeballs detection
    happy_eyeballs_detection(begin, end);

    using connector_type = std::tuple<std::function<void()>, bool>;
    std::vector<connector_type> connectors;

    for (; begin != end; begin++) {
      auto sock = boost::make_local_shared<Stream>(stream.get_executor());

      context->socket_.emplace_back(sock);

      auto conn_func = [this, iter = begin, &stream, context, executor, sock,
                        connect_condition]() mutable {
        do_connect<Stream, Handler, decltype(executor), Iterator,
                   ConnectCondition, ResultType>(
            iter, stream, context, executor, sock, connect_condition);
      };

      auto v4 = begin->endpoint().address().is_v4();

      connectors.emplace_back(connector_type{conn_func, v4});
    }

    for (auto &[conn_func, v4] : connectors) {
      if (use_happy_eyeball && v4) {
        using namespace std::chrono_literals;
        using net::steady_timer;

        // ipv4 delay 200ms.
        auto timer =
            boost::make_local_shared<steady_timer>(stream.get_executor());

        const auto delay = 200ms;

        timer->expires_after(delay);
        timer->async_wait([timer, conn_func = std::move(conn_func),
                           context]([[maybe_unused]] auto error) {
          if (context->flag_)
            return;
          conn_func();
        });
      } else {
        conn_func();
      }
    }
  }

  template <typename Stream, typename Iterator, typename Handler,
            typename ConnectCondition>
  void operator()(Handler &&handler, Stream *s, Iterator begin, Iterator end,
                  ConnectCondition connect_condition) {
    do_async_connect(std::move(handler), *s, begin, end, connect_condition);
  }

  template <typename Stream, typename EndpointSequence, typename Handler,
            typename ConnectCondition>
  void operator()(Handler &&handler, Stream *s,
                  const EndpointSequence &endpoints,
                  ConnectCondition connect_condition) {
    auto begin = endpoints.begin();
    auto end = endpoints.end();
    using Iterator = decltype(begin);

    do_async_connect<Stream, Handler, Iterator, ConnectCondition,
                     typename Stream::endpoint_type>(
        std::move(handler), *s, begin, end, connect_condition);
  }
};
} // namespace detail

template <typename Protocol, typename Executor,
          typename Iterator, typename ConnectHandler>
inline auto async_connect(
    net::basic_stream_socket<Protocol, Executor>& s,
    Iterator begin,
    ConnectHandler handler = net::default_completion_token_t<Executor>(),
    typename net::enable_if<!net::is_endpoint_sequence<Iterator>::value>::type* = 0)
{
    return net::async_initiate<
        ConnectHandler,
        void(boost::system::error_code, Iterator)
    >(
        detail::initiate_do_connect{},
        handler,
        &s,
        begin,
        Iterator(),
        detail::default_connect_condition{}
    );
}

template <typename Protocol, typename Executor, typename Iterator,
          typename ConnectHandler = net::default_completion_token_t<Executor>>
auto async_connect(
    net::basic_stream_socket<Protocol, Executor>& s,
    Iterator begin,
    Iterator end,
    ConnectHandler&& handler = net::default_completion_token_t<Executor>())
{
    return net::async_initiate<
        ConnectHandler,
        void(boost::system::error_code, Iterator)
    >(
        detail::initiate_do_connect{},
        handler,
        &s,
        begin,
        end,
        detail::default_connect_condition{}
    );
}

template <typename Protocol, typename Executor, typename EndpointSequence,
          typename ConnectHandler = net::default_completion_token_t<Executor>>
auto async_connect(
    net::basic_stream_socket<Protocol, Executor>& s,
    const EndpointSequence& endpoints,
    ConnectHandler&& handler = net::default_completion_token_t<Executor>(),
    typename net::enable_if<net::is_endpoint_sequence<EndpointSequence>::value>::type* = 0)
{
    using SocketType = net::basic_stream_socket<Protocol, Executor>;
    using HandlerType = void(boost::system::error_code, typename SocketType::endpoint_type);

    return net::async_initiate<
        ConnectHandler,
        HandlerType
    >(
        detail::initiate_do_connect{},
        handler,
        &s,
        endpoints,
        detail::default_connect_condition{}
    );
}

template <typename Protocol, typename Executor, typename Iterator,
          typename ConnectCondition,
          typename ConnectHandler = net::default_completion_token_t<Executor>>
auto async_connect(
    net::basic_stream_socket<Protocol, Executor>& s,
    Iterator begin,
    ConnectCondition connect_condition,
    ConnectHandler&& handler = net::default_completion_token_t<Executor>(),
    typename net::enable_if<!net::is_endpoint_sequence<Iterator>::value>::type* = 0)
{
    using HandlerType = void(boost::system::error_code, Iterator);

    return net::async_initiate<
        ConnectHandler,
        HandlerType
    >(
        detail::initiate_do_connect{},
        handler,
        &s,
        begin,
        Iterator(),
        connect_condition
    );
}

template <typename Protocol, typename Executor, typename EndpointSequence,
          typename Iterator, typename ConnectCondition,
          typename ConnectHandler = net::default_completion_token_t<Executor>>
auto async_connect(
    net::basic_stream_socket<Protocol, Executor>& s,
    Iterator begin,
    Iterator end,
    ConnectCondition connect_condition,
    ConnectHandler&& handler = net::default_completion_token_t<Executor>())
{
    using HandlerType = void(boost::system::error_code, Iterator);

    return net::async_initiate<
        ConnectHandler,
        HandlerType
    >(
        detail::initiate_do_connect{},
        handler,
        &s,
        begin,
        end,
        connect_condition
    );
}

template <typename Protocol, typename Executor, typename EndpointSequence,
          typename ConnectCondition,
          typename ConnectHandler = net::default_completion_token_t<Executor>>
auto async_connect(
    net::basic_stream_socket<Protocol, Executor>& s,
    const EndpointSequence& endpoints,
    ConnectCondition connect_condition,
    ConnectHandler&& handler = net::default_completion_token_t<Executor>(),
    typename net::enable_if<net::is_endpoint_sequence<EndpointSequence>::value>::type* = 0)
{
    using SocketType = net::basic_stream_socket<Protocol, Executor>;
    using HandlerType = void(boost::system::error_code, typename SocketType::endpoint_type);

    return net::async_initiate<ConnectHandler, HandlerType>(
        detail::initiate_do_connect{},
        handler,
        &s,
        endpoints,
        connect_condition
    );
}

} // namespace asio_util

#endif // INCLUDE__2023_10_18__ASYNC_CONNECT_HPP

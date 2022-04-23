//
// async_connect.hpp
// ~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include <boost/asio/dispatch.hpp>
#include <boost/asio/connect.hpp>

#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/associated_cancellation_slot.hpp>

#include <boost/smart_ptr/local_shared_ptr.hpp>
#include <boost/smart_ptr/make_local_shared.hpp>
#include <boost/smart_ptr/weak_ptr.hpp>

#include <iterator>
#include <atomic>
#include <utility>
#include <memory>
#include <vector>
#include <any>
#include <type_traits>


namespace asio_util {

	namespace detail {
		template <typename Stream, typename Handler>
		struct connect_context
		{
			connect_context(Handler&& h)
				: handler_(std::move(h))
			{}

			std::atomic_int flag_;
			std::atomic_int num_;
			Handler handler_;
			std::vector<boost::local_shared_ptr<Stream>> socket_;
		};

		template <typename Handler, typename ResultType>
		void do_result(Handler&& h, const boost::system::error_code& error, ResultType&& result)
		{
			h(error, result);
		}

		template <typename Stream, typename Handler,
			typename Iterator, typename ResultType = void>
		void callback(Handler&& handler, Iterator& begin, const boost::system::error_code& error)
		{
			auto executor = boost::asio::get_associated_executor(handler);
			boost::asio::post(executor, [error, h = std::move(handler), begin]() mutable
			{
				if constexpr (std::is_same_v<ResultType, typename Stream::endpoint_type>)
					do_result(h, error, *begin);
				if constexpr (!std::is_same_v<ResultType, typename Stream::endpoint_type>)
					do_result(h, error, begin);
			});
		}

		struct default_connect_condition
		{
			template <typename Stream, typename Endpoint>
			bool operator()(const boost::system::error_code&, Stream&, const Endpoint&)
			{
				return true;
			}
		};

		struct initiate_do_connect
		{
			bool use_happy_eyeball = false;
			int reject = 0;

			template <typename Stream, typename Endpoint, typename ConnectCondition>
			bool check_condition(const boost::system::error_code& ec,
				Stream& stream, Endpoint& endp, ConnectCondition connect_condition)
			{
				bool ret = connect_condition(ec, stream, endp);
				if (!ret) reject++;
				return ret;
			}

			template <typename Stream, typename Handler, typename Iterator,
				typename ConnectCondition, typename ResultType = void>
			void do_async_connect(Handler handler, Stream& stream,
				Iterator begin, Iterator end,
				ConnectCondition connect_condition)
			{
				auto context = boost::make_local_shared<connect_context<Stream, Handler>>(std::move(handler));

				auto cs = boost::asio::get_associated_cancellation_slot(context->handler_);
				if (cs.is_connected())
				{
					boost::weak_ptr<connect_context<Stream, Handler>> weak_ptr = context;
					cs.assign([weak_ptr](boost::asio::cancellation_type_t) mutable
					{
						auto context = weak_ptr.lock();
						if (!context)
							return;

						auto& sockets = context->socket_;
						for (auto& t : sockets)
						{
							if (!t)
								continue;

							boost::system::error_code ignore_ec;
							t->cancel(ignore_ec);
						}
					});
				}

				context->flag_ = false;
				context->num_ = std::distance(begin, end);
				if (context->num_ == 0)
				{
					auto error = boost::system::error_code(boost::asio::error::no_data);
					callback<Stream, Handler, Iterator, ResultType>(std::move(context->handler_), begin, error);
					return;
				}

				// happy eyeballs detection
				{
					bool has_a = false, has_aaaa = false;

					for (auto begin_ = begin; begin_ != end; begin_++)
					{
						const auto& addr = begin_->endpoint().address();

						has_aaaa |= boost::asio::ip::address(addr).is_v6();
						has_a |= boost::asio::ip::address(addr).is_v4();
					}

					if (has_aaaa && has_a)
						use_happy_eyeball = true;
				}

				std::vector<std::pair<std::function<void()>, bool>> connectors;

				for (; begin != end; begin++)
				{
					auto sock = boost::make_local_shared<Stream>(stream.get_executor());
					context->socket_.emplace_back(sock);

					auto func = [this, begin, &stream, context, sock, h = &context->handler_, &connect_condition]() mutable
					{
						if (!check_condition({}, *sock, *begin, connect_condition))
						{
							if (reject == context->num_)
							{
								auto error = boost::system::error_code(boost::asio::error::not_found);
								callback<Stream, Handler, Iterator, ResultType>(std::forward<Handler>(*h), begin, error);
							}

							return;
						}

						sock->async_connect(*begin,
						[&stream, context, begin, sock, h]
						(const boost::system::error_code& error) mutable
						{
							if (!error)
							{
								if (context->flag_.exchange(true))
									return;

								stream = std::move(*sock);
							}

							context->num_--;
							bool is_last = context->num_ == 0;

							if (error)
							{
								if (context->flag_ || !is_last)
									return;
							}


							auto& sockets = context->socket_;
							for (auto& t : sockets)
							{
								if (!t)
									continue;
								boost::system::error_code ignore_ec;
								t->cancel(ignore_ec);
							}

							callback<Stream, Handler, Iterator, ResultType>(std::forward<Handler>(*h), begin, error);
						});
					};

					connectors.emplace_back(std::make_pair(func, begin->endpoint().address().is_v4()));
				}

				for (auto& [conn_func, v4] : connectors)
				{
					if (use_happy_eyeball && v4)
					{
						using namespace std::chrono_literals;
						using boost::asio::steady_timer;

						// ipv4 delay 200ms.
						auto delay_timer = boost::make_local_shared<steady_timer>(stream.get_executor());
						auto& timer = *delay_timer;

						timer.expires_from_now(200ms);
						timer.async_wait([delay_timer, conn_func = std::move(conn_func), context]
						([[maybe_unused]] const boost::system::error_code& error)
						{
							if (context->flag_)
								return;
							conn_func();
						});
					}
					else
					{
						conn_func();
					}
				}
			}

			template <typename Stream, typename Iterator,
				typename Handler, typename ConnectCondition>
			void operator()(Handler&& handler, Stream* s,
				Iterator begin, Iterator end, ConnectCondition connect_condition)
			{
				do_async_connect(std::move(handler), *s, begin, end, connect_condition);
			}

			template <typename Stream, typename EndpointSequence,
				typename Handler, typename ConnectCondition>
			void operator()(Handler&& handler, Stream* s,
				const EndpointSequence& endpoints, ConnectCondition connect_condition)
			{
				auto begin = endpoints.begin();
				auto end = endpoints.end();
				using Iterator = decltype(begin);

				do_async_connect<Stream, Handler, Iterator, ConnectCondition,
					typename Stream::endpoint_type>(std::move(handler), *s,
						begin, end, connect_condition);
			}
		};
	}

	template <typename Stream,
	typename Iterator, typename ConnectHandler>
		BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(ConnectHandler,
		void(boost::system::error_code, Iterator))
	async_connect(Stream& s, Iterator begin,
		BOOST_ASIO_MOVE_ARG(ConnectHandler) handler,
		typename boost::asio::enable_if<
			!boost::asio::is_endpoint_sequence<Iterator>::value>::type* = 0)
	{
		return boost::asio::async_initiate<ConnectHandler,
			void(boost::system::error_code, Iterator)>
			(detail::initiate_do_connect{}, handler, &s,
				begin, Iterator(),
				detail::default_connect_condition{});
	}

	template <typename Stream,
		typename Iterator, typename ConnectHandler>
		inline BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(ConnectHandler,
			void(boost::system::error_code, Iterator))
		async_connect(Stream& s, Iterator begin, Iterator end,
			BOOST_ASIO_MOVE_ARG(ConnectHandler) handler)
	{
		return boost::asio::async_initiate<ConnectHandler,
			void(boost::system::error_code, Iterator)>
			(detail::initiate_do_connect{}, handler, &s,
				begin, end,
				detail::default_connect_condition{});
	}

	template <typename Stream,
		typename EndpointSequence, typename ConnectHandler>
		inline BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(ConnectHandler,
			void(boost::system::error_code, typename Stream::endpoint_type))
		async_connect(Stream& s, const EndpointSequence& endpoints,
			BOOST_ASIO_MOVE_ARG(ConnectHandler) handler,
			typename boost::asio::enable_if<
				boost::asio::is_endpoint_sequence<EndpointSequence>::value>::type* = 0)
	{
		return boost::asio::async_initiate<ConnectHandler,
			void(boost::system::error_code, typename Stream::endpoint_type)>
			(detail::initiate_do_connect{}, handler, &s, endpoints,
				detail::default_connect_condition{});
	}

	template <typename Stream,
	typename Iterator, typename ConnectHandler, typename ConnectCondition>
		BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(ConnectHandler,
		void(boost::system::error_code, Iterator))
	async_connect(Stream& s, Iterator begin, ConnectCondition connect_condition,
		BOOST_ASIO_MOVE_ARG(ConnectHandler) handler,
		typename boost::asio::enable_if<
			!boost::asio::is_endpoint_sequence<Iterator>::value>::type* = 0)
	{
		return boost::asio::async_initiate<ConnectHandler,
			void(boost::system::error_code, Iterator)>
			(detail::initiate_do_connect{}, handler, &s,
				begin, Iterator(),
				connect_condition);
	}

	template <typename Stream,
		typename Iterator, typename ConnectHandler, typename ConnectCondition>
		inline BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(ConnectHandler,
			void(boost::system::error_code, Iterator))
		async_connect(Stream& s, Iterator begin, Iterator end, ConnectCondition connect_condition,
			BOOST_ASIO_MOVE_ARG(ConnectHandler) handler)
	{
		return boost::asio::async_initiate<ConnectHandler,
			void(boost::system::error_code, Iterator)>
			(detail::initiate_do_connect{}, handler, &s,
				begin, end,
				connect_condition);
	}

	template <typename Stream,
		typename EndpointSequence, typename ConnectHandler, typename ConnectCondition>
		inline BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(ConnectHandler,
			void(boost::system::error_code, typename Stream::endpoint_type))
		async_connect(Stream& s, const EndpointSequence& endpoints, ConnectCondition connect_condition,
			BOOST_ASIO_MOVE_ARG(ConnectHandler) handler,
			typename boost::asio::enable_if<
				boost::asio::is_endpoint_sequence<EndpointSequence>::value>::type* = 0)
	{
		return boost::asio::async_initiate<ConnectHandler,
			void(boost::system::error_code, typename Stream::endpoint_type)>
			(detail::initiate_do_connect{}, handler, &s, endpoints, connect_condition);
	}
}

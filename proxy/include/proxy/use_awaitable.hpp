//
// use_awaitable.hpp
// ~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2023_10_18__USE_AWAITABLE_HPP
#define INCLUDE__2023_10_18__USE_AWAITABLE_HPP


#include <boost/type_traits.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>

namespace asio_util
{
	template <typename Executor = boost::asio::any_io_executor>
	struct asio_use_awaitable_t : public boost::asio::use_awaitable_t<Executor>
	{
		constexpr asio_use_awaitable_t()
			: boost::asio::use_awaitable_t<Executor>()
		{}

		constexpr asio_use_awaitable_t(const char* file_name,
			int line, const char* function_name)
			: boost::asio::use_awaitable_t<Executor>(file_name, line, function_name)
		{}

		inline boost::asio::redirect_error_t<
			typename boost::decay<decltype(boost::asio::use_awaitable_t<Executor>())>::type>
			operator[](boost::system::error_code& ec) const noexcept
		{
			return boost::asio::redirect_error(boost::asio::use_awaitable_t<Executor>(), ec);
		}
	};
}

//
// net_awaitable usage:
//
// boost::system::error_code ec;
// stream.async_read(buffer, net_awaitable[ec]);
// or
// stream.async_read(buffer, net_awaitable);
//

// Executor is any_io_executor
[[maybe_unused]] inline constexpr
	asio_util::asio_use_awaitable_t<> net_awaitable;

// Executor is boost::asio::io_context::executor_type
[[maybe_unused]] inline constexpr
	asio_util::asio_use_awaitable_t<
		boost::asio::io_context::executor_type> ioc_awaitable;

#endif // INCLUDE__2023_10_18__USE_AWAITABLE_HPP

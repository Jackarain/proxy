//
// use_awaitable.hpp
// ~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include <boost/type_traits.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>

namespace asio_util
{
	struct asio_use_awaitable_t : public boost::asio::use_awaitable_t<>
	{
		constexpr asio_use_awaitable_t()
			: boost::asio::use_awaitable_t<>()
		{}

		constexpr asio_use_awaitable_t(const char* file_name,
			int line, const char* function_name)
			: boost::asio::use_awaitable_t<>(file_name, line, function_name)
		{}

		inline boost::asio::redirect_error_t<
			typename boost::decay<decltype(boost::asio::use_awaitable)>::type>
			operator[](boost::system::error_code& ec) const noexcept
		{
			return boost::asio::redirect_error(boost::asio::use_awaitable, ec);
		}
	};
}

namespace boost::asio {
	template <typename R, typename... Args>
	class async_result<asio_util::asio_use_awaitable_t, R(Args...)>
	{
	public:
		typedef typename detail::awaitable_handler<
			any_io_executor, typename decay<Args>::type...> handler_type;
		typedef typename handler_type::awaitable_type return_type;

		template <typename Initiation, typename... InitArgs>
#if defined(__APPLE_CC__) && (__clang_major__ == 13)
		__attribute__((noinline))
#endif // defined(__APPLE_CC__) && (__clang_major__ == 13)
			static handler_type* do_init(
				detail::awaitable_frame_base<any_io_executor>* frame, Initiation& initiation,
				asio_util::asio_use_awaitable_t u, InitArgs&... args)
		{
			(void)u;
			BOOST_ASIO_HANDLER_LOCATION((u.file_name_, u.line_, u.function_name_));
			handler_type handler(frame->detach_thread());
			std::move(initiation)(std::move(handler), std::move(args)...);
			return nullptr;
		}

		template <typename Initiation, typename... InitArgs>
		static return_type initiate(Initiation initiation,
			asio_util::asio_use_awaitable_t u, InitArgs... args)
		{
			co_await[&](auto* frame)
			{
				return do_init(frame, initiation, u, args...);
			};

			for (;;) {} // Never reached.
#if defined(_MSC_VER)
			co_return dummy_return<typename return_type::value_type>();
#endif // defined(_MSC_VER)
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

[[maybe_unused]] inline constexpr asio_util::asio_use_awaitable_t net_awaitable;

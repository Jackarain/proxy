//
// test_net_socket_option.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/net_socket_option.hpp"

#include <boost/asio/error.hpp>
#include <boost/test/unit_test.hpp>

#include <cerrno>
#include <cstdint>
#include <optional>

#if !defined(BOOST_WINDOWS)
# include <netinet/in.h>
# include <netinet/tcp.h>
# include <sys/socket.h>
# include <unistd.h>
#endif


namespace {

	using namespace proxy;

	BOOST_AUTO_TEST_SUITE(net_socket_option_suite)

#if !defined(BOOST_WINDOWS)

	// 创建一个用于测试的 TCP socket, 失败时返回 -1.
	int make_test_socket()
	{
		return ::socket(AF_INET, SOCK_STREAM, 0);
	}

	BOOST_AUTO_TEST_CASE(make_error_code_sets_and_clears)
	{
		boost::system::error_code ec = net::error::operation_aborted;

		make_error_code(ec, false);
		BOOST_TEST(!ec);

		errno = EACCES;
		make_error_code(ec, true);
		BOOST_TEST(ec.value() == EACCES);
		bool system_category = ec.category() == net::error::get_system_category();
		BOOST_TEST(system_category);

		errno = 0;
		make_error_code(ec, true);
		BOOST_TEST(ec.value() == 0);
		BOOST_TEST(!ec);
	}

	BOOST_AUTO_TEST_CASE(set_tcp_nodelay_toggles_option)
	{
		int fd = make_test_socket();
		BOOST_REQUIRE(fd >= 0);

		auto on = set_tcp_nodelay(fd, true);
		BOOST_TEST(on.has_value());
		BOOST_TEST(on.value());

		int value = 0;
		socklen_t len = sizeof(value);
		BOOST_TEST(::getsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &value, &len) == 0);
		BOOST_TEST(value == 1);

		auto off = set_tcp_nodelay(fd, false);
		BOOST_TEST(off.has_value());
		len = sizeof(value);
		BOOST_TEST(::getsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &value, &len) == 0);
		BOOST_TEST(value == 0);

		::close(fd);
	}

	BOOST_AUTO_TEST_CASE(set_tcp_nodelay_reports_error_on_bad_fd)
	{
		auto result = set_tcp_nodelay(-1, true);
		BOOST_TEST(result.has_error());
		BOOST_TEST(result.error().value() == EBADF);
	}

	BOOST_AUTO_TEST_CASE(set_tcp_buffer_sizes_applies_both_directions)
	{
		int fd = make_test_socket();
		BOOST_REQUIRE(fd >= 0);

		BOOST_TEST(set_tcp_buffer_sizes(fd, 65536, 65536).has_value());

		int value = 0;
		socklen_t len = sizeof(value);
		BOOST_TEST(::getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &value, &len) == 0);
		BOOST_TEST(value > 0);
		len = sizeof(value);
		BOOST_TEST(::getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &value, &len) == 0);
		BOOST_TEST(value > 0);

		// 0 表示跳过对应方向的设置, 仍应成功.
		BOOST_TEST(set_tcp_buffer_sizes(fd, 0, 0).has_value());

		::close(fd);
	}

	BOOST_AUTO_TEST_CASE(apply_so_mark_skips_when_unset)
	{
		int fd = make_test_socket();
		BOOST_REQUIRE(fd >= 0);

		std::optional<uint32_t> none;
		BOOST_TEST(apply_so_mark(fd, none).has_value());

		::close(fd);
	}

# if defined(__linux__)
	BOOST_AUTO_TEST_CASE(set_tcp_quickack_succeeds_on_socket)
	{
		int fd = make_test_socket();
		BOOST_REQUIRE(fd >= 0);

		BOOST_TEST(set_tcp_quickack(fd, true).has_value());
		BOOST_TEST(set_tcp_quickack(fd, false).has_value());

		::close(fd);
	}
# endif

#endif

	BOOST_AUTO_TEST_SUITE_END()

} // namespace

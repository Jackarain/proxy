//
// test_rfc9298_capsule.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/rfc9298_capsule.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/connect_pipe.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/readable_pipe.hpp>
#include <boost/asio/writable_pipe.hpp>
#include <boost/asio/write.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/test/unit_test.hpp>

#include <array>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>


namespace {

	using namespace proxy;

	// 把 capsule 的 type 与 length 头部依次写入 head, 返回写入的字节数.
	size_t write_capsule_header(uint64_t type, uint64_t length, uint8_t* head)
	{
		size_t off = 0;
		off += varint_int_encode(type, head + off);
		off += varint_int_encode(length, head + off);
		return off;
	}

	BOOST_AUTO_TEST_SUITE(rfc9298_capsule_suite)

	BOOST_AUTO_TEST_CASE(varint_encoded_length_boundaries)
	{
		BOOST_TEST(varint_encoded_length(0) == 1u);
		BOOST_TEST(varint_encoded_length(0x3F) == 1u);
		BOOST_TEST(varint_encoded_length(0x40) == 2u);
		BOOST_TEST(varint_encoded_length(0x3FFF) == 2u);
		BOOST_TEST(varint_encoded_length(0x4000) == 4u);
		BOOST_TEST(varint_encoded_length(0x3FFFFFFF) == 4u);
		BOOST_TEST(varint_encoded_length(0x40000000) == 8u);
		BOOST_TEST(varint_encoded_length(0x3FFFFFFFFFFFFFFFULL) == 8u);
	}

	BOOST_AUTO_TEST_CASE(varint_round_trip_all_widths)
	{
		const uint64_t values[] = {
			0, 1, 0x3F, 0x40, 0x7F, 0x3FFF, 0x4000, 0x12345678,
			0x3FFFFFFF, 0x40000000, 0x3FFFFFFFFFFFFFFFULL,
		};

		for (auto value : values)
		{
			std::array<uint8_t, 8> buf{};
			auto written = varint_int_encode(value, buf.data());
			BOOST_TEST(written == varint_encoded_length(value));

			auto decoded = varint_int_decode(buf.data());
			BOOST_TEST(decoded.first == written);
			BOOST_TEST(decoded.second == value);
		}
	}

	BOOST_AUTO_TEST_CASE(varint_decode_bounded_rejects_short_input)
	{
		std::array<uint8_t, 8> buf{};
		auto written = varint_int_encode(0x4000, buf.data());
		BOOST_TEST(written == 4u);

		auto zero = varint_int_decode_bounded(buf.data(), 0);
		BOOST_TEST(zero.first == 0u);
		BOOST_TEST(zero.second == 0u);

		auto short_input = varint_int_decode_bounded(buf.data(), 3);
		BOOST_TEST(short_input.first == 0u);
		BOOST_TEST(short_input.second == 0u);

		auto full = varint_int_decode_bounded(buf.data(), 4);
		BOOST_TEST(full.first == 4u);
		BOOST_TEST(full.second == 0x4000u);
	}

	BOOST_AUTO_TEST_CASE(datagram_capsule_header_layout)
	{
		// 空负载: length = 1, 变长整数占 1 字节, 头部共 3 字节.
		BOOST_TEST(datagram_capsule_header_size(0) == 3u);

		// length = 1 + 100 = 101 > 0x3F, 变长整数占 2 字节, 头部共 4 字节.
		BOOST_TEST(datagram_capsule_header_size(100) == 4u);
		BOOST_TEST(datagram_capsule_header_size(300) == 4u);

		// length = 1 + 65535 = 65536, 变长整数占 4 字节, 头部共 6 字节.
		BOOST_TEST(datagram_capsule_header_size(65535) == 6u);

		std::array<uint8_t, 16> buf{};
		auto written = encode_datagram_capsule_header(100, buf.data());
		BOOST_TEST(written == datagram_capsule_header_size(100));

		auto type_field = varint_int_decode(buf.data());
		BOOST_TEST(type_field.first == 1u);
		BOOST_TEST(type_field.second == udp_proxy_capsule_type);

		auto length_field = varint_int_decode(buf.data() + type_field.first);
		BOOST_TEST(length_field.first == 2u);
		BOOST_TEST(length_field.second == 101u);

		auto ctx_field = varint_int_decode(
			buf.data() + type_field.first + length_field.first);
		BOOST_TEST(ctx_field.first == 1u);
		BOOST_TEST(ctx_field.second == 0u);
	}

	BOOST_AUTO_TEST_CASE(datagram_capsule_round_trip)
	{
		const std::string payload(300, 'x');

		std::vector<uint8_t> out(
			datagram_capsule_header_size(payload.size()) + payload.size());
		auto written = encode_datagram_capsule(
			payload.data(), payload.size(), out.data(), out.size());

		BOOST_TEST(written == out.size());

		// 跳过 type/length 头部, value = context id + payload.
		auto type_len = varint_encoded_length(udp_proxy_capsule_type);
		auto len_len = varint_encoded_length(1 + payload.size());

		std::string_view value(
			reinterpret_cast<const char*>(out.data()) + type_len + len_len,
			written - type_len - len_len);

		auto decoded = decode_datagram_capsule(value);
		BOOST_TEST(std::string(decoded) == payload);

		// vector 重载与 string_view 重载行为一致.
		std::vector<char> value_vec(value.begin(), value.end());
		BOOST_TEST(std::string(decode_datagram_capsule(value_vec)) == payload);
	}

	BOOST_AUTO_TEST_CASE(datagram_capsule_rejects_small_buffer)
	{
		const std::string payload(10, 'y');

		std::vector<uint8_t> exact(
			datagram_capsule_header_size(payload.size()) + payload.size());
		BOOST_TEST(encode_datagram_capsule(
			payload.data(), payload.size(), exact.data(), exact.size()) ==
			exact.size());

		std::vector<uint8_t> one_shorter(exact.size() - 1);
		BOOST_TEST(encode_datagram_capsule(
			payload.data(), payload.size(), one_shorter.data(),
			one_shorter.size()) == 0u);

		// 空负载也需要容纳 3 字节头部.
		std::vector<uint8_t> no_space(2);
		BOOST_TEST(encode_datagram_capsule(
			payload.data(), 0, no_space.data(), no_space.size()) == 0u);
	}

	BOOST_AUTO_TEST_CASE(datagram_capsule_value_edge_cases)
	{
		// 空 value.
		BOOST_TEST(decode_datagram_capsule(std::string_view{}).empty());

		// context id 非 0.
		const char nonzero_ctx[] = { 0x01, 'a' };
		BOOST_TEST(decode_datagram_capsule(
			std::string_view(nonzero_ctx, sizeof(nonzero_ctx))).empty());

		// 只有 context id, 没有 payload 仍是合法空数据报.
		const char ctx_only[] = { 0x00 };
		std::string_view empty_payload;
		BOOST_TEST(decode_datagram_capsule(
			std::string_view(ctx_only, sizeof(ctx_only)), empty_payload));
		BOOST_TEST(empty_payload.empty());

		std::array<uint8_t, 8> empty_capsule{};
		auto empty_size = encode_datagram_capsule(
			"", 0, empty_capsule.data(), empty_capsule.size());
		BOOST_TEST(empty_size == datagram_capsule_header_size(0));
		BOOST_TEST(decode_datagram_message(std::string_view(
			reinterpret_cast<const char*>(empty_capsule.data()), empty_size),
			empty_payload));
		BOOST_TEST(empty_payload.empty());
	}

	BOOST_AUTO_TEST_CASE(datagram_message_round_trip)
	{
		const std::string payload(300, 'z');
		std::vector<uint8_t> encoded(
			datagram_capsule_header_size(payload.size()) + payload.size());
		auto encoded_len = encode_datagram_capsule(payload.data(),
			payload.size(), encoded.data(), encoded.size());

		std::string_view message(
			reinterpret_cast<const char*>(encoded.data()), encoded_len);
		std::string_view decoded;
		BOOST_TEST(decode_datagram_message(message, decoded));
		BOOST_TEST(decoded == payload);

		auto trailing = message;
		trailing = message.substr(0, message.size() - 1);
		BOOST_TEST(!decode_datagram_message(trailing, decoded));

		std::vector<uint8_t> wrong_type = encoded;
		wrong_type[0] = 1;
		BOOST_TEST(!decode_datagram_message(std::string_view(
			reinterpret_cast<const char*>(wrong_type.data()),
			wrong_type.size()), decoded));
	}

	BOOST_AUTO_TEST_CASE(connect_udp_upgrade_response_validation)
	{
		using namespace boost::beast::http;

		response<empty_body> res{ status::switching_protocols, 11 };
		res.set(field::connection, "keep-alive, Upgrade");
		res.set(field::upgrade, " Connect-UDP ");
		BOOST_TEST(is_connect_udp_upgrade_response(res));

		res.set(field::connection, "notupgrade");
		BOOST_TEST(!is_connect_udp_upgrade_response(res));

		res.set(field::connection, "Upgrade");
		res.set(field::upgrade, "websocket");
		BOOST_TEST(!is_connect_udp_upgrade_response(res));

		res.set(field::upgrade, "connect-udp");
		res.insert(field::upgrade, "connect-udp");
		BOOST_TEST(!is_connect_udp_upgrade_response(res));

		response<empty_body> not_switching{ status::ok, 11 };
		not_switching.set(field::connection, "Upgrade");
		not_switching.set(field::upgrade, "connect-udp");
		BOOST_TEST(!is_connect_udp_upgrade_response(not_switching));
	}

	BOOST_AUTO_TEST_CASE(connect_udp_upgrade_request_validation)
	{
		using namespace boost::beast::http;

		request<empty_body> req{ verb::get, "/", 11 };
		req.set(field::connection, "keep-alive, Upgrade");
		req.set(field::upgrade, " Connect-UDP ");
		BOOST_TEST(is_connect_udp_upgrade_request(req));

		req.set(field::connection, "notupgrade");
		BOOST_TEST(!is_connect_udp_upgrade_request(req));

		req.set(field::connection, "Upgrade");
		req.method(verb::post);
		BOOST_TEST(!is_connect_udp_upgrade_request(req));

		req.method(verb::get);
		req.set(field::upgrade, "websocket");
		BOOST_TEST(!is_connect_udp_upgrade_request(req));

		req.set(field::upgrade, "connect-udp");
		req.insert(field::upgrade, "connect-udp");
		BOOST_TEST(!is_connect_udp_upgrade_request(req));
	}

	BOOST_AUTO_TEST_CASE(read_capsule_from_stream_valid)
	{
		net::io_context ioc;
		net::readable_pipe reader(ioc);
		net::writable_pipe writer(ioc);
		net::connect_pipe(reader, writer);

		const std::string payload = "hello dns";
		std::vector<uint8_t> encoded(
			datagram_capsule_header_size(payload.size()) + payload.size());
		auto encoded_len = encode_datagram_capsule(
			payload.data(), payload.size(), encoded.data(), encoded.size());
		net::write(writer, net::buffer(encoded.data(), encoded_len));

		boost::system::error_code ec;
		uint64_t type = 0;
		std::vector<char> value;
		bool finished = false;

		net::co_spawn(reader.get_executor(),
			[&]() -> net::awaitable<void>
			{
				auto result = co_await read_capsule_from_stream(reader, ec);
				type = result.first;
				value = std::move(result.second);
				finished = true;
				co_return;
			}, net::detached);

		ioc.run();

		BOOST_TEST(finished);
		BOOST_TEST(!ec);
		BOOST_TEST(type == udp_proxy_capsule_type);
		BOOST_TEST(std::string(decode_datagram_capsule(value)) == payload);
	}

	BOOST_AUTO_TEST_CASE(read_capsule_consumes_buffered_bytes_first)
	{
		net::io_context ioc;
		net::readable_pipe reader(ioc);
		net::writable_pipe writer(ioc);
		net::connect_pipe(reader, writer);

		const std::string payload = "buffered capsule payload";
		std::vector<uint8_t> encoded(
			datagram_capsule_header_size(payload.size()) + payload.size());
		auto encoded_len = encode_datagram_capsule(
			payload.data(), payload.size(), encoded.data(), encoded.size());
		auto buffered_len = datagram_capsule_header_size(payload.size()) + 5;
		BOOST_TEST(buffered_len < encoded_len);

		boost::beast::flat_buffer buffer;
		buffer.commit(net::buffer_copy(
			buffer.prepare(buffered_len),
			net::buffer(encoded.data(), buffered_len)));
		net::write(writer,
			net::buffer(encoded.data() + buffered_len, encoded_len - buffered_len));

		boost::system::error_code ec;
		uint64_t type = 0;
		std::vector<char> value;
		bool finished = false;

		net::co_spawn(reader.get_executor(),
			[&]() -> net::awaitable<void>
			{
				auto result = co_await
					read_capsule_from_stream(reader, buffer, ec);
				type = result.first;
				value = std::move(result.second);
				finished = true;
				co_return;
			}, net::detached);

		ioc.run();

		BOOST_TEST(finished);
		BOOST_TEST(!ec);
		BOOST_TEST(buffer.size() == 0u);
		BOOST_TEST(type == udp_proxy_capsule_type);
		BOOST_TEST(std::string(decode_datagram_capsule(value)) == payload);
	}

	BOOST_AUTO_TEST_CASE(read_capsule_from_buffer_without_stream_read)
	{
		net::io_context ioc;
		net::readable_pipe reader(ioc);

		const std::string payload = "fully buffered";
		std::vector<uint8_t> encoded(
			datagram_capsule_header_size(payload.size()) + payload.size());
		auto encoded_len = encode_datagram_capsule(
			payload.data(), payload.size(), encoded.data(), encoded.size());

		boost::beast::flat_buffer buffer;
		buffer.commit(net::buffer_copy(
			buffer.prepare(encoded_len), net::buffer(encoded)));

		boost::system::error_code ec;
		bool finished = false;

		net::co_spawn(reader.get_executor(),
			[&]() -> net::awaitable<void>
			{
				auto result = co_await
					read_capsule_from_stream(reader, buffer, ec);
				BOOST_TEST(result.first == udp_proxy_capsule_type);
				BOOST_TEST(std::string(
					decode_datagram_capsule(result.second)) == payload);
				finished = true;
				co_return;
			}, net::detached);

		ioc.run();

		BOOST_TEST(finished);
		BOOST_TEST(!ec);
		BOOST_TEST(buffer.size() == 0u);
	}

	BOOST_AUTO_TEST_CASE(read_capsule_rejects_oversized_length)
	{
		net::io_context ioc;
		net::readable_pipe reader(ioc);
		net::writable_pipe writer(ioc);
		net::connect_pipe(reader, writer);

		std::array<uint8_t, 8> head{};
		auto head_len = write_capsule_header(
			udp_proxy_capsule_type, datagram_capsule_max_length + 1,
			head.data());
		net::write(writer, net::buffer(head.data(), head_len));

		boost::system::error_code ec;
		bool finished = false;

		net::co_spawn(reader.get_executor(),
			[&]() -> net::awaitable<void>
			{
				co_await read_capsule_from_stream(reader, ec);
				finished = true;
				co_return;
			}, net::detached);

		ioc.run();

		BOOST_TEST(finished);
		BOOST_TEST(ec == net::error::invalid_argument);
	}

	BOOST_AUTO_TEST_SUITE_END()

} // namespace

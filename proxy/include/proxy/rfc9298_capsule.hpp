//
// rfc9298_capsule.hpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2026_10_13__RFC9298_CAPSULE_HPP
#define INCLUDE__2026_10_13__RFC9298_CAPSULE_HPP


#include "proxy/use_awaitable.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <utility>


namespace proxy {

	namespace net = boost::asio;

	static constexpr uint64_t udp_proxy_capsule_type = 0x00; // RFC 9297 DATAGRAM


	//////////////////////////////////////////////////////////////////////////
	// QUIC 可变长度整数编码/解码 (RFC 9298 capsule 协议)

	// 获取 QUIC 变长整数编码后的实际字节数
	constexpr size_t varint_encoded_length(uint64_t val)
	{
		if (val <= 0x3F)       return 1;
		if (val <= 0x3FFF)     return 2;
		if (val <= 0x3FFFFFFF) return 4;
		return 8;
	}

	// 将 value 编码为 QUIC 变长整数, 写入 buf, 返回写入的字节数.
	inline size_t varint_int_encode(uint64_t value, uint8_t* buf) noexcept
	{
		if (value < 0x40)
		{
			buf[0] = static_cast<uint8_t>(value);
			return 1;
		}
		else if (value < 0x4000)
		{
			value |= 0x4000;
			buf[0] = static_cast<uint8_t>(value >> 8);
			buf[1] = static_cast<uint8_t>(value);
			return 2;
		}
		else if (value < 0x40000000)
		{
			value |= 0x80000000;
			buf[0] = static_cast<uint8_t>(value >> 24);
			buf[1] = static_cast<uint8_t>(value >> 16);
			buf[2] = static_cast<uint8_t>(value >> 8);
			buf[3] = static_cast<uint8_t>(value);
			return 4;
		}
		else
		{
			value |= 0xC000000000000000ULL;
			buf[0] = static_cast<uint8_t>(value >> 56);
			buf[1] = static_cast<uint8_t>(value >> 48);
			buf[2] = static_cast<uint8_t>(value >> 40);
			buf[3] = static_cast<uint8_t>(value >> 32);
			buf[4] = static_cast<uint8_t>(value >> 24);
			buf[5] = static_cast<uint8_t>(value >> 16);
			buf[6] = static_cast<uint8_t>(value >> 8);
			buf[7] = static_cast<uint8_t>(value);
			return 8;
		}
	}

	// 从 buf 解码一个 QUIC 变长整数, 返回 {字节数, 值}.
	inline std::pair<size_t, uint64_t> varint_int_decode(const uint8_t* buf) noexcept
	{
		uint8_t prefix = buf[0] >> 6;
		if (prefix == 0)
			return { 1, buf[0] };
		else if (prefix == 1)
		{
			uint64_t v = (static_cast<uint64_t>(buf[0]) << 8) | buf[1];
			return { 2, v & 0x3FFF };
		}
		else if (prefix == 2)
		{
			uint64_t v = (static_cast<uint64_t>(buf[0]) << 24) |
				(static_cast<uint64_t>(buf[1]) << 16) |
				(static_cast<uint64_t>(buf[2]) << 8) |
				buf[3];
			return { 4, v & 0x3FFFFFFF };
		}
		else
		{
			uint64_t v = (static_cast<uint64_t>(buf[0]) << 56) |
				(static_cast<uint64_t>(buf[1]) << 48) |
				(static_cast<uint64_t>(buf[2]) << 40) |
				(static_cast<uint64_t>(buf[3]) << 32) |
				(static_cast<uint64_t>(buf[4]) << 24) |
				(static_cast<uint64_t>(buf[5]) << 16) |
				(static_cast<uint64_t>(buf[6]) << 8) |
				buf[7];
			return { 8, v & 0x3FFFFFFFFFFFFFFF };
		}
	}

	// 从 buf 解码一个 QUIC 变长整数并做边界检查, 返回 {字节数, 值}.
	// 输入长度不足以容纳该变长整数时返回 {0, 0}, 调用方需处理该情况.
	inline std::pair<size_t, uint64_t> varint_int_decode_bounded(
		const uint8_t* buf, size_t len) noexcept
	{
		if (len == 0)
			return { 0, 0 };

		size_t need = static_cast<size_t>(1) << (buf[0] >> 6);
		if (len < need)
			return { 0, 0 };

		return varint_int_decode(buf);
	}

	// 从流中读取一个 QUIC 变长整数 (varint), 用于 RFC 9298 capsule 协议解析.
	// 注意: 需要调用方已包含 proxy_stream.hpp 和 use_awaitable.hpp.
	// 返回读取到的值, ec 非零表示读取失败.
	template <typename Stream>
	inline net::awaitable<uint64_t>
	read_varint_from_stream(Stream& stream, boost::system::error_code& ec)
	{
		uint8_t buf[8];

		co_await net::async_read(stream, net::buffer(buf, 1), net_awaitable[ec]);
		if (ec)
			co_return 0;

		uint8_t prefix = buf[0] >> 6;
		size_t len = 1 << prefix;

		if (len > 1)
		{
			co_await net::async_read(stream, net::buffer(buf + 1, len - 1),
				net_awaitable[ec]);
			if (ec)
				co_return 0;
		}

		auto [consumed, value] = varint_int_decode(buf);
		(void)consumed;

		co_return value;
	}

	//////////////////////////////////////////////////////////////////////////
	// RFC 9297/9298 DATAGRAM capsule 封装与解析

	// capsule 长度上限, 与 UDP 报文最大长度一致.
	inline constexpr size_t datagram_capsule_max_length = 65535;

	// 计算 DATAGRAM capsule 头部字节数. 头部由三个 varint 组成:
	//   capsule type, capsule length (1 + payload), context ID (固定为 0).
	constexpr size_t datagram_capsule_header_size(size_t payload_len) noexcept
	{
		return varint_encoded_length(udp_proxy_capsule_type) +
			varint_encoded_length(1 + payload_len) +
			varint_encoded_length(0);
	}

	// 将 DATAGRAM capsule 头部编码写入 out, 返回写入字节数.
	inline size_t
	encode_datagram_capsule_header(size_t payload_len, uint8_t* out) noexcept
	{
		size_t pos = 0;
		pos += varint_int_encode(udp_proxy_capsule_type, out + pos);
		pos += varint_int_encode(1 + payload_len, out + pos);
		pos += varint_int_encode(0, out + pos);
		return pos;
	}

	// 将 UDP payload 完整封装为 DATAGRAM capsule 写入 out.
	// 返回写入的总字节数; out 空间不足时返回 0.
	inline size_t encode_datagram_capsule(const void* data, size_t len,
		uint8_t* out, size_t out_size) noexcept
	{
		auto header = datagram_capsule_header_size(len);
		if (len > out_size || header > out_size - len)
			return 0;

		encode_datagram_capsule_header(len, out);
		std::memcpy(out + header, data, len);
		return header + len;
	}

	// 解析 DATAGRAM capsule 的 value, 返回其中的 UDP payload 视图.
	// context ID 非 0、长度不足或 payload 为空时返回空视图.
	inline std::string_view
	decode_datagram_capsule(std::string_view value) noexcept
	{
		auto data = reinterpret_cast<const uint8_t*>(value.data());
		auto [ctx_id_len, ctx_id] = varint_int_decode_bounded(
			data, value.size());

		if (ctx_id_len == 0 || ctx_id != 0 || ctx_id_len >= value.size())
			return {};

		return value.substr(ctx_id_len);
	}

}


#endif // INCLUDE__2026_10_13__RFC9298_CAPSULE_HPP

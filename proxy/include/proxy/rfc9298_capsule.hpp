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
#include <boost/asio/error.hpp>
#include <boost/asio/read.hpp>
#include <boost/system/error_code.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/status.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>


namespace proxy {

	namespace net = boost::asio;

	static constexpr uint64_t udp_proxy_capsule_type = 0x00; // RFC 9297 DATAGRAM

	inline constexpr std::string_view connect_udp_path_prefix =
		"/.well-known/masque/udp/";

	inline std::string encode_connect_udp_host(std::string_view host)
	{
		static constexpr char hex[] = "0123456789ABCDEF";

		std::string encoded;
		encoded.reserve(host.size());
		for (unsigned char ch : host)
		{
			if (ch == ':' || ch == '%')
			{
				encoded.push_back('%');
				encoded.push_back(hex[ch >> 4]);
				encoded.push_back(hex[ch & 0x0F]);
			}
			else
			{
				encoded.push_back(static_cast<char>(ch));
			}
		}
		return encoded;
	}

	inline std::string format_connect_udp_path(
		std::string_view host, uint16_t port)
	{
		std::string path(connect_udp_path_prefix);
		path += encode_connect_udp_host(host);
		path.push_back('/');
		path += std::to_string(port);
		path.push_back('/');
		return path;
	}

	namespace detail {

		inline char ascii_lower(char c) noexcept
		{
			return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
		}

		inline bool ascii_iequals(std::string_view lhs,
			std::string_view rhs) noexcept
		{
			if (lhs.size() != rhs.size())
				return false;
			for (size_t i = 0; i < lhs.size(); ++i)
			{
				if (ascii_lower(lhs[i]) != ascii_lower(rhs[i]))
					return false;
			}
			return true;
		}

		inline std::string_view trim_ows(std::string_view value) noexcept
		{
			while (!value.empty() &&
				(value.front() == ' ' || value.front() == '\t'))
				value.remove_prefix(1);
			while (!value.empty() &&
				(value.back() == ' ' || value.back() == '\t'))
				value.remove_suffix(1);
			return value;
		}

		inline bool header_has_token(std::string_view value,
			std::string_view token) noexcept
		{
			while (!value.empty())
			{
				auto comma = value.find(',');
				auto item = trim_ows(value.substr(0, comma));
				if (ascii_iequals(item, token))
					return true;
				if (comma == std::string_view::npos)
					break;
				value.remove_prefix(comma + 1);
			}
			return false;
		}

	} // namespace detail

	// 校验 RFC 9298 HTTP/1.1 CONNECT-UDP 成功响应.
	template <typename Body, typename Fields>
	inline bool is_connect_udp_upgrade_response(
		const boost::beast::http::response<Body, Fields>& resp) noexcept
	{
		using namespace boost::beast::http;

		if (resp.result() != status::switching_protocols ||
			resp.count(field::upgrade) != 1)
			return false;

		auto upgrade = resp.find(field::upgrade);
		if (!detail::ascii_iequals(
			detail::trim_ows(upgrade->value()), "connect-udp"))
			return false;

		for (auto it = resp.begin(); it != resp.end(); ++it)
		{
			if (it->name() == field::connection &&
				detail::header_has_token(it->value(), "upgrade"))
				return true;
		}

		return false;
	}

	// 校验 RFC 9298 HTTP/1.1 CONNECT-UDP 请求. Capsule-Protocol 是
	// SHOULD 头, Upgrade token 本身已足以标识 Capsule Protocol.
	template <typename Body, typename Fields>
	inline bool is_connect_udp_upgrade_request(
		const boost::beast::http::request<Body, Fields>& req) noexcept
	{
		using namespace boost::beast::http;

		if (req.method() != verb::get ||
			req.count(field::upgrade) != 1)
			return false;

		auto upgrade = req.find(field::upgrade);
		if (!detail::ascii_iequals(
			detail::trim_ows(upgrade->value()), "connect-udp"))
			return false;

		for (auto it = req.begin(); it != req.end(); ++it)
		{
			if (it->name() == field::connection &&
				detail::header_has_token(it->value(), "upgrade"))
				return true;
		}

		return false;
	}


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
	template <typename Stream, typename DynamicBuffer>
	inline net::awaitable<void> read_capsule_bytes(Stream& stream,
		DynamicBuffer& buffer, void* data, std::size_t len,
		boost::system::error_code& ec)
	{
		auto* out = static_cast<uint8_t*>(data);
		auto copied = net::buffer_copy(
			net::buffer(out, len), buffer.data());
		buffer.consume(copied);

		if (copied < len)
		{
			co_await net::async_read(stream,
				net::buffer(out + copied, len - copied),
				net_awaitable[ec]);
		}

		co_return;
	}

	template <typename Stream, typename DynamicBuffer>
	inline net::awaitable<uint64_t>
	read_varint_from_stream(Stream& stream, DynamicBuffer& buffer,
		boost::system::error_code& ec)
	{
		uint8_t buf[8];

		co_await read_capsule_bytes(stream, buffer, buf, 1, ec);
		if (ec)
			co_return 0;

		uint8_t prefix = buf[0] >> 6;
		size_t len = 1 << prefix;

		if (len > 1)
		{
			co_await read_capsule_bytes(
				stream, buffer, buf + 1, len - 1, ec);
			if (ec)
				co_return 0;
		}

		co_return varint_int_decode(buf).second;
	}

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

	inline constexpr size_t max_udp_payload_size = 65535;

	// capsule value 由 context ID 和 UDP payload 组成.
	inline constexpr size_t datagram_capsule_max_length =
		max_udp_payload_size + 1;

	// 最大 UDP payload 对应的完整 DATAGRAM capsule 长度.
	inline constexpr size_t datagram_capsule_max_size =
		varint_encoded_length(udp_proxy_capsule_type) +
		varint_encoded_length(datagram_capsule_max_length) +
		datagram_capsule_max_length;

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
		if (len > max_udp_payload_size)
			return 0;

		auto header = datagram_capsule_header_size(len);
		if (len > out_size || header > out_size - len)
			return 0;

		encode_datagram_capsule_header(len, out);
		std::memcpy(out + header, data, len);
		return header + len;
	}

	// 解析 DATAGRAM capsule 的 value, 输出其中的 UDP payload 视图.
	// context ID 非 0 或长度不足时返回 false.
	inline bool decode_datagram_capsule(std::string_view value,
		std::string_view& payload) noexcept
	{
		auto data = reinterpret_cast<const uint8_t*>(value.data());
		auto [ctx_id_len, ctx_id] = varint_int_decode_bounded(
			data, value.size());

		if (ctx_id_len == 0 || ctx_id != 0 || ctx_id_len > value.size())
			return false;

		payload = value.substr(ctx_id_len);
		return true;
	}

	// 解析 DATAGRAM capsule 的 value, 返回其中的 UDP payload 视图.
	// context ID 非 0 或长度不足时返回空视图.
	inline std::string_view
	decode_datagram_capsule(std::string_view value) noexcept
	{
		std::string_view payload;
		return decode_datagram_capsule(value, payload) ? payload :
			std::string_view{};
	}

	// 解析 DATAGRAM capsule 的 value, 输出其中的 UDP payload 视图.
	inline bool decode_datagram_capsule(const std::vector<char>& value,
		std::string_view& payload) noexcept
	{
		return decode_datagram_capsule(
			std::string_view(value.data(), value.size()), payload);
	}

	// 解析 DATAGRAM capsule 的 value, 返回其中的 UDP payload 视图.
	inline std::string_view
	decode_datagram_capsule(const std::vector<char>& value) noexcept
	{
		return decode_datagram_capsule(
			std::string_view(value.data(), value.size()));
	}

	// 解析一条完整的 WebSocket binary 消息, 其中必须恰好包含一个
	// DATAGRAM capsule. 返回其中的 UDP payload 视图.
	inline bool decode_datagram_message(std::string_view message,
		std::string_view& payload) noexcept
	{
		auto data = reinterpret_cast<const uint8_t*>(message.data());
		auto [type_len, type] = varint_int_decode_bounded(
			data, message.size());
		if (type_len == 0 || type != udp_proxy_capsule_type ||
			type_len >= message.size())
			return false;

		auto [length_len, capsule_length] = varint_int_decode_bounded(
			data + type_len, message.size() - type_len);
		if (length_len == 0)
			return false;

		auto value_offset = type_len + length_len;
		if (capsule_length != message.size() - value_offset)
			return false;

		return decode_datagram_capsule(
			message.substr(value_offset, capsule_length), payload);
	}

	// 从流中读取一个完整 capsule, 返回 {capsule type, capsule value}.
	// capsule 长度超过上限时置 ec 为 invalid_argument.
	template <typename Stream, typename DynamicBuffer>
	inline net::awaitable<std::pair<uint64_t, std::vector<char>>>
	read_capsule_from_stream(Stream& stream, DynamicBuffer& buffer,
		boost::system::error_code& ec)
	{
		using capsule_pair = std::pair<uint64_t, std::vector<char>>;
		ec = {};

		auto capsule_type = co_await
			read_varint_from_stream(stream, buffer, ec);
		if (ec)
			co_return capsule_pair{};

		auto capsule_length = co_await
			read_varint_from_stream(stream, buffer, ec);
		if (ec)
			co_return capsule_pair{};

		if (capsule_length > datagram_capsule_max_length)
		{
			ec = net::error::invalid_argument;
			co_return capsule_pair{};
		}

		std::vector<char> value(static_cast<size_t>(capsule_length));
		if (capsule_length > 0)
		{
			co_await read_capsule_bytes(
				stream, buffer, value.data(), value.size(), ec);
			if (ec)
				co_return capsule_pair{};
		}

		co_return capsule_pair{ capsule_type, std::move(value) };
	}

	template <typename Stream>
	inline net::awaitable<std::pair<uint64_t, std::vector<char>>>
	read_capsule_from_stream(Stream& stream, boost::system::error_code& ec)
	{
		using capsule_pair = std::pair<uint64_t, std::vector<char>>;

		auto capsule_type = co_await read_varint_from_stream(stream, ec);
		if (ec)
			co_return capsule_pair{};

		auto capsule_length = co_await read_varint_from_stream(stream, ec);
		if (ec)
			co_return capsule_pair{};

		if (capsule_length > datagram_capsule_max_length)
		{
			ec = net::error::invalid_argument;
			co_return capsule_pair{};
		}

		std::vector<char> value(static_cast<size_t>(capsule_length));
		if (capsule_length > 0)
		{
			co_await net::async_read(
				stream, net::buffer(value), net_awaitable[ec]);
			if (ec)
				co_return capsule_pair{};
		}

		co_return capsule_pair{ capsule_type, std::move(value) };
	}

}


#endif // INCLUDE__2026_10_13__RFC9298_CAPSULE_HPP

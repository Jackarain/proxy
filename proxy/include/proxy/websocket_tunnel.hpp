//
// websocket_tunnel.hpp
// ~~~~~~~~~~~~~~~~~~~~
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2026_10_25__WEBSOCKET_TUNNEL_HPP
#define INCLUDE__2026_10_25__WEBSOCKET_TUNNEL_HPP

#include "proxy/use_awaitable.hpp"
#include "proxy/strutil.hpp"

#include <boost/asio/associated_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>

#include <boost/system/error_code.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace proxy {
namespace ws_tunnel {

	namespace net = boost::asio;
	namespace beast = boost::beast;
	namespace http = beast::http;
	namespace websocket = beast::websocket;

	// WebSocket CONNECT 隧道协议. 所有数据帧均为 binary, 第一个字节为操作码.
	inline constexpr std::string_view proxy_connect_header = "Proxy-Connect";
	inline constexpr std::string_view proxy_connect_header_legacy = "PROXY_CONNECT";
	inline constexpr std::string_view proxy_connect_subprotocol = "proxy-connect.v1";
	inline constexpr std::string_view proxy_connect_path = "/.well-known/proxy/connect";

	inline constexpr uint8_t opcode_data = 0x00;
	inline constexpr uint8_t opcode_fin_write = 0x01;
	inline constexpr uint8_t opcode_abort = 0x02;

	// 单个 WebSocket 消息的上限. 首字节为操作码, 因而有效载荷为 64 KiB - 1.
	inline constexpr std::size_t max_message_size = 64 * 1024;
	inline constexpr std::size_t max_data_size = max_message_size - 1;
	inline constexpr std::size_t max_authority_size = 1024;

	inline char ascii_lower(char c) noexcept
	{
		return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
	}

	inline bool ascii_iequals(std::string_view a, std::string_view b) noexcept
	{
		if (a.size() != b.size())
			return false;
		for (std::size_t i = 0; i < a.size(); ++i)
		{
			if (ascii_lower(a[i]) != ascii_lower(b[i]))
				return false;
		}
		return true;
	}

	inline std::string_view trim_ows(std::string_view value) noexcept
	{
		while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
			value.remove_prefix(1);
		while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
			value.remove_suffix(1);
		return value;
	}

	// 检查逗号分隔的 HTTP 头值中是否存在指定 token.
	inline bool header_has_token(std::string_view value, std::string_view token) noexcept
	{
		while (!value.empty())
		{
			auto pos = value.find(',');
			auto item = trim_ows(value.substr(0, pos));
			if (ascii_iequals(item, token))
				return true;
			if (pos == std::string_view::npos)
				break;
			value.remove_prefix(pos + 1);
		}
		return false;
	}

	inline bool valid_host_char(char c) noexcept
	{
		auto uc = static_cast<unsigned char>(c);
		return uc > 0x20 && uc < 0x7f && c != '/' && c != '@' &&
			c != '?' && c != '#' && c != '[' && c != ']';
	}

	// 解析 Proxy-Connect 的 host:port. IPv6 必须使用 [addr]:port 形式.
	inline bool parse_authority(std::string_view value,
		std::string& host, uint16_t& port) noexcept
	{
		value = trim_ows(value);
		if (value.empty() || value.size() > max_authority_size)
			return false;

		std::string_view host_part;
		std::string_view port_part;

		if (value.front() == '[')
		{
			auto close = value.find(']');
			if (close == std::string_view::npos || close <= 1 ||
				close + 2 > value.size() || value[close + 1] != ':')
				return false;

			host_part = value.substr(1, close - 1);
			port_part = value.substr(close + 2);
			if (host_part.find(':') == std::string_view::npos)
				return false;
		}
		else
		{
			auto pos = value.rfind(':');
			if (pos == std::string_view::npos || pos == 0 ||
				value.find(':') != pos)
				return false;

			host_part = value.substr(0, pos);
			port_part = value.substr(pos + 1);
		}

		if (host_part.empty() || port_part.empty())
			return false;
		if (!std::all_of(host_part.begin(), host_part.end(), valid_host_char))
			return false;

		uint32_t port_value = 0;
		auto [ptr, ec] = std::from_chars(
			port_part.data(), port_part.data() + port_part.size(), port_value);
		if (ec != std::errc{} || ptr != port_part.data() + port_part.size() ||
			port_value == 0 || port_value > 65535)
			return false;

		host.assign(host_part);
		port = static_cast<uint16_t>(port_value);
		return true;
	}

	inline std::string format_authority(std::string_view host, uint16_t port)
	{
		std::string authority;
		authority.reserve(host.size() + 16);
		if (host.find(':') != std::string_view::npos)
		{
			authority.push_back('[');
			authority.append(host);
			authority.push_back(']');
		}
		else
		{
			authority.append(host);
		}
		authority.push_back(':');
		authority.append(std::to_string(port));
		return authority;
	}

	struct client_handshake_options
	{
		std::string host;
		std::string target{ proxy_connect_path };
		std::string authority;
		std::string username;
		std::string password;
		http::fields extra_headers;
	};

	// 将 WebSocket binary 消息适配为面向字节流的 AsyncReadStream/AsyncWriteStream.
	template <typename NextLayer>
	class stream
	{
		using ws_type = websocket::stream<std::decay_t<NextLayer>>;

		struct io_result
		{
			boost::system::error_code ec;
			std::size_t bytes = 0;
		};

		class initiate_read_some
		{
		public:
			template <typename Handler, typename MutableBufferSequence>
			void operator()(Handler&& handler, stream* self,
				MutableBufferSequence buffers) const
			{
				auto executor = net::get_associated_executor(
					handler, self->get_executor());
				net::co_spawn(executor,
					[self, buffers = std::move(buffers),
						handler = std::forward<Handler>(handler)]() mutable
						-> net::awaitable<void>
					{
						auto result = co_await self->do_read_some(buffers);
						handler(result.ec, result.bytes);
					}, net::detached);
			}
		};

		class initiate_write_some
		{
		public:
			template <typename Handler, typename ConstBufferSequence>
			void operator()(Handler&& handler, stream* self,
				ConstBufferSequence buffers) const
			{
				auto executor = net::get_associated_executor(
					handler, self->get_executor());
				net::co_spawn(executor,
					[self, buffers = std::move(buffers),
						handler = std::forward<Handler>(handler)]() mutable
						-> net::awaitable<void>
					{
						auto result = co_await self->do_write_some(buffers);
						handler(result.ec, result.bytes);
					}, net::detached);
			}
		};

		class initiate_shutdown_send
		{
		public:
			template <typename Handler>
			void operator()(Handler&& handler, stream* self) const
			{
				auto executor = net::get_associated_executor(
					handler, self->get_executor());
				net::co_spawn(executor,
					[self, handler = std::forward<Handler>(handler)]() mutable
						-> net::awaitable<void>
					{
						auto ec = co_await self->do_shutdown_send();
						handler(ec);
					}, net::detached);
			}
		};

	public:
		using next_layer_type = std::decay_t<NextLayer>;
		using lowest_layer_type = typename next_layer_type::lowest_layer_type;
		using executor_type = typename ws_type::executor_type;

		template <typename... Args>
		explicit stream(Args&&... args)
			: ws_(std::make_unique<ws_type>(
				std::forward<Args>(args)...))
		{
		}

		stream(stream&& other) noexcept = default;
		stream& operator=(stream&& other) noexcept = default;

		stream(const stream&) = delete;
		stream& operator=(const stream&) = delete;
		~stream() = default;

		executor_type get_executor() noexcept
		{
			return ws_->get_executor();
		}

		next_layer_type& next_layer() noexcept
		{
			return ws_->next_layer();
		}

		const next_layer_type& next_layer() const noexcept
		{
			return ws_->next_layer();
		}

		lowest_layer_type& lowest_layer() noexcept
		{
			return ws_->next_layer().lowest_layer();
		}

		const lowest_layer_type& lowest_layer() const noexcept
		{
			return ws_->next_layer().lowest_layer();
		}

		bool is_open() const noexcept
		{
			return ws_->is_open() && lowest_layer().is_open();
		}

		void close(boost::system::error_code& ec) noexcept
		{
			lowest_layer().close(ec);
		}

		net::awaitable<boost::system::error_code> async_accept(
			const http::request<http::string_body>& req)
		{
			boost::system::error_code ec;
			configure();
			set_accept_decorator();
			co_await ws_->async_accept(req, net_awaitable[ec]);
			co_return ec;
		}

		// 将 HTTP 解析阶段预读的数据与序列化后的升级请求一起交给 Beast,
		// 避免客户端在等待 101 前发送的首批 WebSocket 帧丢失.
		template <typename ConstBufferSequence>
		net::awaitable<boost::system::error_code> async_accept(
			const http::request<http::string_body>& req,
			const ConstBufferSequence& prefix)
		{
			boost::system::error_code ec;
			beast::flat_buffer buffer;

			std::string wire;
			wire.reserve(256 + req.body().size());
			wire.append(req.method_string());
			wire.push_back(' ');
			wire.append(req.target());
			wire.append(" HTTP/1.1\r\n");
			for (const auto& field : req.base())
			{
				wire.append(field.name_string());
				wire.append(": ");
				wire.append(field.value());
				wire.append("\r\n");
			}
			wire.append("\r\n");
			wire.append(req.body());

			buffer.commit(net::buffer_copy(
				buffer.prepare(wire.size()), net::buffer(wire)));

			auto prefix_size = net::buffer_size(prefix);
			if (prefix_size > 0)
			{
				buffer.commit(net::buffer_copy(
					buffer.prepare(prefix_size), prefix));
			}

			configure();
			set_accept_decorator();
			co_await ws_->async_accept(buffer.data(), net_awaitable[ec]);
			co_return ec;
		}

		net::awaitable<boost::system::error_code> async_handshake(
			client_handshake_options options)
		{
			boost::system::error_code ec;
			auto host = std::move(options.host);
			auto target = std::move(options.target);

			configure();
			ws_->set_option(websocket::stream_base::decorator(
				[options = std::move(options)](websocket::request_type& req)
				{
					req.set(proxy_connect_header, options.authority);
					req.set(http::field::sec_websocket_protocol,
						proxy_connect_subprotocol);

					if (!options.username.empty())
					{
						auto userinfo = options.username + ":" + options.password;
						req.set(http::field::proxy_authorization,
							"Basic " + strutil::base64_encode(userinfo));
					}

					for (const auto& field : options.extra_headers)
						req.insert(field.name_string(), field.value());
				}));

			websocket::response_type response;
			co_await ws_->async_handshake(response, host, target,
				net_awaitable[ec]);
			if (ec)
				co_return ec;

			auto protocol = response[http::field::sec_websocket_protocol];
			if (!header_has_token(protocol, proxy_connect_subprotocol))
			{
				co_return make_error_code(
					boost::system::errc::protocol_error);
			}

			co_return ec;
		}

		template <typename MutableBufferSequence, typename Handler>
		auto async_read_some(const MutableBufferSequence& buffers,
			Handler&& handler)
		{
			return net::async_initiate<Handler,
				void(boost::system::error_code, std::size_t)>(
					initiate_read_some{}, handler, this, buffers);
		}

		template <typename ConstBufferSequence, typename Handler>
		auto async_write_some(const ConstBufferSequence& buffers,
			Handler&& handler)
		{
			return net::async_initiate<Handler,
				void(boost::system::error_code, std::size_t)>(
					initiate_write_some{}, handler, this, buffers);
		}

		template <typename Handler>
		auto async_shutdown_send(Handler&& handler)
		{
			return net::async_initiate<Handler,
				void(boost::system::error_code)>(
					initiate_shutdown_send{}, handler, this);
		}

		bool write_closed() const noexcept
		{
			return write_closed_;
		}

	private:
		void configure()
		{
			ws_->binary(true);
			ws_->read_message_max(max_message_size);
		}

		void set_accept_decorator()
		{
			ws_->set_option(websocket::stream_base::decorator(
				[](websocket::response_type& res)
				{
					res.set(http::field::sec_websocket_protocol,
						proxy_connect_subprotocol);
				}));
		}

		template <typename MutableBufferSequence>
		net::awaitable<io_result> do_read_some(MutableBufferSequence buffers)
		{
			boost::system::error_code ec;

			if (read_pos_ < read_size_)
			{
				auto bytes = copy_to_user(buffers);
				co_return io_result{ ec, bytes };
			}

			if (read_eof_)
				co_return io_result{ net::error::eof, 0 };

			for (;;)
			{
				read_buffer_.consume(read_buffer_.size());
				co_await ws_->async_read(read_buffer_, net_awaitable[ec]);
				if (ec)
				{
					if (ec == websocket::error::closed)
						ec = net::error::eof;
					co_return io_result{ ec, 0 };
				}

				if (!ws_->got_binary())
				{
					co_return io_result{ make_error_code(
						boost::system::errc::protocol_error), 0 };
				}

				auto size = read_buffer_.size();
				if (size == 0)
					continue;
					if (size > max_message_size)
				{
					co_return io_result{ make_error_code(
						boost::system::errc::message_size), 0 };
				}

					ensure_buffers();
				net::buffer_copy(
						net::buffer(read_data_.get(), size),
					read_buffer_.data());

				auto opcode = static_cast<uint8_t>(read_data_[0]);

				if (opcode == opcode_abort)
					co_return io_result{ net::error::connection_aborted, 0 };

				if (opcode == opcode_fin_write)
				{
					if (size != 1)
					{
						co_return io_result{ make_error_code(
							boost::system::errc::protocol_error), 0 };
					}
					read_eof_ = true;
					co_return io_result{ net::error::eof, 0 };
				}

				if (opcode != opcode_data)
				{
					co_return io_result{ make_error_code(
						boost::system::errc::protocol_error), 0 };
				}

				if (size == 1)
					continue;

				read_size_ = size;
				read_pos_ = 1;
				auto bytes = copy_to_user(buffers);
				co_return io_result{ ec, bytes };
			}
		}

		template <typename ConstBufferSequence>
		net::awaitable<io_result> do_write_some(ConstBufferSequence buffers)
		{
			boost::system::error_code ec;
			if (write_closed_)
				co_return io_result{ net::error::broken_pipe, 0 };

			auto size = std::min<std::size_t>(
				net::buffer_size(buffers), max_data_size);
			if (size == 0)
				co_return io_result{ ec, 0 };

			ensure_buffers();
			write_opcode_ = opcode_data;
			net::buffer_copy(
				net::buffer(write_data_.get(), size),
				beast::buffers_prefix(size, buffers));

			std::array<net::const_buffer, 2> message{
				net::buffer(&write_opcode_, 1),
				net::buffer(write_data_.get(), size) };
			co_await ws_->async_write(message, net_awaitable[ec]);
			if (ec)
				co_return io_result{ ec, 0 };

			co_return io_result{ ec, size };
		}

		net::awaitable<boost::system::error_code> do_shutdown_send()
		{
			boost::system::error_code ec;
			if (write_closed_)
				co_return ec;

			write_closed_ = true;
			write_opcode_ = opcode_fin_write;
			co_await ws_->async_write(net::buffer(&write_opcode_, 1),
				net_awaitable[ec]);
			co_return ec;
		}

		template <typename MutableBufferSequence>
		std::size_t copy_to_user(const MutableBufferSequence& buffers) noexcept
		{
			auto available = read_size_ - read_pos_;
			auto bytes = net::buffer_copy(buffers,
				net::buffer(read_data_.get() + read_pos_, available));
			read_pos_ += bytes;
			return bytes;
		}

		void ensure_buffers()
		{
			if (!read_data_)
			{
				read_data_ = std::make_unique_for_overwrite<
					char[]>(max_message_size);
			}
			if (!write_data_)
			{
				write_data_ = std::make_unique_for_overwrite<
					char[]>(max_message_size);
			}
		}

	private:
		std::unique_ptr<ws_type> ws_;
		beast::flat_buffer read_buffer_;
		std::unique_ptr<char[]> read_data_;
		std::size_t read_size_{ 0 };
		std::size_t read_pos_{ 0 };
		std::unique_ptr<char[]> write_data_;
		char write_opcode_{ 0 };
		bool read_eof_{ false };
		bool write_closed_{ false };
	};

} // namespace ws_tunnel
} // namespace proxy

#endif // INCLUDE__2026_10_25__WEBSOCKET_TUNNEL_HPP

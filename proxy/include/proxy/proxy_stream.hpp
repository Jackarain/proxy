//
// Copyright (C) 2022 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#ifndef INCLUDE__2023_11_25__PROXY_SOCKET_HPP
#define INCLUDE__2023_11_25__PROXY_SOCKET_HPP


#include "proxy/variant_stream.hpp"
#include "proxy/scramble.hpp"
#include "proxy/tcp_socket.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/buffer.hpp>

#include <cstdint>
#include <type_traits>


namespace util {

	using tcp = net::ip::tcp;               // from <boost/asio/ip/tcp.hpp>

	//////////////////////////////////////////////////////////////////////////
	// socket 层次关系
	// tcp_socket 是建立在 boost::beast::tcp_stream 之上的包装，它能提供超时和限速等功能.
	// proxy_stream 则建立在 tcp_socket 之上，它能为 tcp_socket 传输的数据提供 scramble / unscramble
	// 等功能.
	// 而 stream 是建立在 proxy_tcp_socket 之上，它提供 ssl/tls 加解密 proxy_tcp_socket 层的数据的功能.
	// 最终由 variant_stream_type 包含 proxy_tcp_socket/ssl_stream 两个的变体定义.

	// proxy_tcp_socket (proxy_stream > tcp_socket)
	// ssl_stream (stream > proxy_tcp_socket)
	// variant_stream_type (proxy_tcp_socket | ssl_stream)

	template <typename Stream>
	class proxy_stream;

	using proxy_tcp_socket = proxy_stream<tcp_socket>;
	using tcp_acceptor = tcp::acceptor;

	using ssl_stream = net::ssl::stream<proxy_tcp_socket>;
	using variant_stream_type = variant_stream<proxy_tcp_socket, ssl_stream>;

	//////////////////////////////////////////////////////////////////////////


	template <typename Stream>
	class proxy_stream final
	{
		proxy_stream(const proxy_stream&) = delete;
		proxy_stream& operator=(proxy_stream const&) = delete;

		class initiate_async_read_some;
		class initiate_async_write_some;

	public:

		using next_layer_type = typename std::remove_reference<Stream>::type;
		using lowest_layer_type = typename next_layer_type::lowest_layer_type;
		using executor_type = typename next_layer_type::executor_type;
		using native_handle_type = typename next_layer_type::native_handle_type;
		using endpoint_type = typename next_layer_type::endpoint_type;
		using protocol_type = typename next_layer_type::protocol_type;

	public:

		template <typename Arg>
		explicit proxy_stream(Arg&& s)
			: next_layer_(std::move(s))
		{}

		explicit proxy_stream(executor_type exec)
			: next_layer_(exec)
		{}

		explicit proxy_stream(net::io_context& ioc)
			: next_layer_(ioc)
		{}

		~proxy_stream() = default;

		proxy_stream& operator=(proxy_stream&& other) noexcept
		{
			next_layer_ = std::move(other.next_layer_);

			scramble_ = std::move(other.scramble_);
			unscramble_ = std::move(other.unscramble_);

			return *this;
		}

		proxy_stream(proxy_stream&& other) noexcept
			: next_layer_(std::move(other.next_layer_))
			, scramble_(std::move(other.scramble_))
			, unscramble_(std::move(other.unscramble_))
		{}

		void set_scramble_key(const std::vector<uint8_t>& key)
		{
			scramble_.set_key(key);
		}

		void set_unscramble_key(const std::vector<uint8_t>& key)
		{
			unscramble_.set_key(key);
		}

		proxy::scramble_stream& scramble()
		{
			return scramble_;
		}

		proxy::scramble_stream& unscramble()
		{
			return unscramble_;
		}

		executor_type get_executor()
		{
			return next_layer_.get_executor();
		}

		Stream& next_layer()
		{
			return next_layer_;
		}

		const Stream& next_layer() const
		{
			return next_layer_;
		}

		lowest_layer_type& lowest_layer()
		{
			return next_layer_.lowest_layer();
		}

		const lowest_layer_type& lowest_layer() const
		{
			return next_layer_.lowest_layer();
		}

		native_handle_type native_handle()
		{
			return next_layer_.lowest_layer().native_handle();
		}

		tcp::endpoint remote_endpoint()
		{
			return next_layer_.lowest_layer().remote_endpoint();
		}

		tcp::endpoint remote_endpoint(boost::system::error_code& ec)
		{
			return next_layer_.lowest_layer().remote_endpoint(ec);
		}

		void shutdown(net::socket_base::shutdown_type what)
		{
			next_layer_.lowest_layer().shutdown(what);
		}

		void shutdown(net::socket_base::shutdown_type what,
			boost::system::error_code& ec)
		{
			next_layer_.lowest_layer().shutdown(what, ec);
		}

		void close()
		{
			next_layer_.close();
		}

		void close(boost::system::error_code& ec)
		{
			next_layer_.close(ec);
		}

		bool is_open() const
		{
			return next_layer_.is_open();
		}

		void open(const protocol_type& protocol = protocol_type())
		{
			next_layer_.open(protocol);
		}

		void open(const protocol_type& protocol, boost::system::error_code& ec)
		{
			next_layer_.open(protocol, ec);
		}

		void bind(const tcp::endpoint& endpoint)
		{
			next_layer_.bind(endpoint);
		}

		void bind(const endpoint_type& endpoint, boost::system::error_code& ec)
		{
			next_layer_.bind(endpoint, ec);
		}

		template <typename SettableSocketOption>
		void set_option(const SettableSocketOption& option)
		{
			next_layer_.set_option(option);
		}

		template <typename SettableSocketOption>
		void set_option(const SettableSocketOption& option,
			boost::system::error_code& ec)
		{
			next_layer_.set_option(option, ec);
		}

		template <typename ConnectHandler>
		auto async_connect(const tcp::endpoint& endpoint, ConnectHandler&& handler)
		{
			return next_layer_.async_connect(endpoint,
				std::forward<ConnectHandler>(handler));
		}

		template <typename MutableBufferSequence, typename ReadHandler>
		auto async_read_some(const MutableBufferSequence& buffers, ReadHandler&& handler)
		{
			return net::async_initiate<ReadHandler,
				void (boost::system::error_code, std::size_t)>(
					initiate_async_read_some(this), handler, buffers);
		}

		template <typename ConstBufferSequence, typename WriteHandler>
		auto async_write_some(const ConstBufferSequence& buffers, WriteHandler&& handler)
		{
			return net::async_initiate<WriteHandler,
				void (boost::system::error_code, std::size_t)>(
					initiate_async_write_some(this), handler, buffers);
		}

		template <typename WaitHandler>
		auto async_wait(net::socket_base::wait_type w, WaitHandler&& handler)
		{
			return next_layer_.async_wait(w, std::forward<WaitHandler>(handler));
		}

	private:
		class initiate_async_read_some
		{
		public:
			typedef typename proxy_stream::executor_type executor_type;

			explicit initiate_async_read_some(proxy_stream* self)
				: self_(self)
			{
			}

			executor_type get_executor() const noexcept
			{
				return self_->get_executor();
			}

			template <typename ReadHandler, typename MutableBufferSequence>
			void operator()(ReadHandler&& handler,
				const MutableBufferSequence& buffers) const
			{
				// 读取数据后, 调用 unscramble_ 立即解密数据.
				self_->next_layer_.async_read_some(buffers,
					[buffers, self_ = self_, handler = std::move(handler)]
						(auto ec, auto bytes) mutable
						{
							if (self_->unscramble_.is_valid()) [[likely]]
							{
								net::mutable_buffer buffer =
									net::detail::buffer_sequence_adapter<
										net::mutable_buffer,
										decltype(buffers)
									>::first(buffers);

								self_->unscramble_.scramble(
									(uint8_t*)buffer.data(), bytes);
							}

							handler(ec, bytes);
						});
			}

		private:
			proxy_stream* self_;
		};

		class initiate_async_write_some
		{
		public:
			typedef typename proxy_stream::executor_type executor_type;

			explicit initiate_async_write_some(proxy_stream* self)
				: self_(self)
			{
			}

			executor_type get_executor() const noexcept
			{
				return self_->get_executor();
			}

			template <typename WriteHandler, typename ConstBufferSequence>
			void operator()(WriteHandler&& handler,
				const ConstBufferSequence& buffers) const
			{
				// 发送数据前, 立即调用 scramble_ 加密数据.
				if (self_->scramble_.is_valid()) [[likely]]
				{
					net::const_buffer buffer =
						net::detail::buffer_sequence_adapter<
							net::const_buffer,
							decltype(buffers)>::first(buffers);

					self_->scramble_.scramble(
						(uint8_t*)buffer.data(), buffer.size());
				}

				// 异步发送数据.
				self_->next_layer_.async_write_some(buffers, std::forward<WriteHandler>(handler));
			}

		private:
			proxy_stream* self_;
		};

	private:
		Stream next_layer_;

		proxy::scramble_stream scramble_;
		proxy::scramble_stream unscramble_;
	};


	//////////////////////////////////////////////////////////////////////////

	inline variant_stream_type init_proxy_stream(
		variant_stream_type& s)
	{
		return variant_stream_type(proxy_tcp_socket(s.get_executor()));
	}

	inline variant_stream_type init_proxy_stream(
		net::any_io_executor executor)
	{
		return variant_stream_type(proxy_tcp_socket(executor));
	}

	inline variant_stream_type init_proxy_stream(
		net::io_context& ioc)
	{
		return variant_stream_type(proxy_tcp_socket(ioc));
	}

	template <typename Stream>
	inline variant_stream_type init_proxy_stream(
		Stream&& s)
	{
		using StreamType = std::decay_t<Stream>;

		if constexpr (std::same_as<StreamType, proxy_tcp_socket>)
			return variant_stream_type(std::move(s));
		else if constexpr (std::same_as<StreamType, ssl_stream>)
			return variant_stream_type(std::move(s));
		else
			return variant_stream_type(proxy_tcp_socket(std::move(s)));
	}

	template <typename Stream>
	inline variant_stream_type init_proxy_stream(
		Stream&& s, net::ssl::context& sslctx)
	{
		if constexpr (std::same_as<std::decay_t<Stream>, tcp::socket>)
		{
			return variant_stream_type(ssl_stream(
				std::forward<tcp::socket>(s), sslctx));
		}
		else {
			return variant_stream_type(ssl_stream(
				proxy_tcp_socket(std::move(s)), sslctx));
		}
	}

}

#endif // INCLUDE__2023_11_25__PROXY_SOCKET_HPP

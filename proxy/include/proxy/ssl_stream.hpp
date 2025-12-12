//
// Copyright (C) 2025 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#ifndef INCLUDE__2025_12_12__SSL_STREAM_HPP
#define INCLUDE__2025_12_12__SSL_STREAM_HPP

#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/executor.hpp>


namespace util {

	namespace net = boost::asio;

	template <typename Stream>
	class ssl_stream
		: public net::ssl::stream_base
	{
	private:
		class initiate_async_handshake;
		class initiate_async_buffered_handshake;
		class initiate_async_shutdown;
		class initiate_async_write_some;
		class initiate_async_read_some;

		public:
		/// The native handle type of the SSL stream.
		typedef SSL* native_handle_type;

		/// Structure for use with deprecated impl_type.
		struct impl_struct
		{
			SSL* ssl;
		};

		/// The type of the next layer.
		typedef net::remove_reference_t<Stream> next_layer_type;

		/// The type of the lowest layer.
		typedef typename next_layer_type::lowest_layer_type lowest_layer_type;

		/// The type of the executor associated with the object.
		typedef typename lowest_layer_type::executor_type executor_type;

		/// Construct a stream.
		template <typename Arg>
		ssl_stream(Arg&& arg, net::ssl::context& ctx)
			: next_layer_(static_cast<Arg&&>(arg)),
			core_(ctx.native_handle(), next_layer_.lowest_layer().get_executor())
		{
		}

		/// Construct a stream from an existing native implementation.
		template <typename Arg>
		ssl_stream(Arg&& arg, native_handle_type handle)
			: next_layer_(static_cast<Arg&&>(arg)),
			core_(handle, next_layer_.lowest_layer().get_executor())
		{
		}

		/// Move-construct a stream from another.
		ssl_stream(ssl_stream&& other)
			: next_layer_(static_cast<Stream&&>(other.next_layer_)),
			core_(static_cast<net::ssl::detail::stream_core&&>(other.core_))
		{
		}

		/// Move-assign a stream from another.
		ssl_stream& operator=(ssl_stream&& other)
		{
			if (this != &other)
			{
				next_layer_ = static_cast<Stream&&>(other.next_layer_);
				core_ = static_cast<net::ssl::detail::stream_core&&>(other.core_);
			}
			return *this;
		}

		/// Destructor.
		~ssl_stream()
		{
		}

		/// Get the executor associated with the object.
		executor_type get_executor() noexcept
		{
			return next_layer_.lowest_layer().get_executor();
		}

		/// Get the underlying implementation in the native type.
		native_handle_type native_handle()
		{
			return core_.engine_.native_handle();
		}

		/// Get a reference to the next layer.
		const next_layer_type& next_layer() const
		{
			return next_layer_;
		}

		/// Get a reference to the next layer.
		next_layer_type& next_layer()
		{
			return next_layer_;
		}

		/// Get a reference to the lowest layer.
		lowest_layer_type& lowest_layer()
		{
			return next_layer_.lowest_layer();
		}

		/// Get a reference to the lowest layer.
		const lowest_layer_type& lowest_layer() const
		{
			return next_layer_.lowest_layer();
		}

		/// Set the peer verification mode.
		void set_verify_mode(net::ssl::verify_mode v)
		{
			boost::system::error_code ec;
			set_verify_mode(v, ec);
			boost::asio::detail::throw_error(ec, "set_verify_mode");
		}

		/// Set the peer verification mode.
		boost::system::error_code set_verify_mode(
			net::ssl::verify_mode v, boost::system::error_code& ec)
		{
			core_.engine_.set_verify_mode(v, ec);
			return ec;
		}

		/// Set the peer verification depth.
		void set_verify_depth(int depth)
		{
			boost::system::error_code ec;
			set_verify_depth(depth, ec);
			net::detail::throw_error(ec, "set_verify_depth");
		}

		/// Set the peer verification depth.
		boost::system::error_code set_verify_depth(
			int depth, boost::system::error_code& ec)
		{
			core_.engine_.set_verify_depth(depth, ec);
			return ec;
		}

		/// Set the callback used to verify peer certificates.
		template <typename VerifyCallback>
		void set_verify_callback(VerifyCallback callback)
		{
			boost::system::error_code ec;
			this->set_verify_callback(callback, ec);
			net::detail::throw_error(ec, "set_verify_callback");
		}

		/// Set the callback used to verify peer certificates.
		template <typename VerifyCallback>
		boost::system::error_code set_verify_callback(VerifyCallback callback,
			boost::system::error_code& ec)
		{
			core_.engine_.set_verify_callback(
				new net::ssl::detail::verify_callback<VerifyCallback>(callback), ec);
			return ec;
		}

		// Get output data to be written to the transport.
		boost::asio::mutable_buffer get_output(const boost::asio::mutable_buffer& data)
		{
			return core_.engine_.get_output(data);
		}

  		// Put input data that was read from the transport.
		boost::asio::const_buffer put_input(const boost::asio::const_buffer& data)
		{
			return core_.engine_.put_input(data);
		}

		/// Perform SSL handshaking.
		void handshake(handshake_type type)
		{
			boost::system::error_code ec;
			handshake(type, ec);
			net::detail::throw_error(ec, "handshake");
		}

		/// Perform SSL handshaking.
		boost::system::error_code handshake(handshake_type type,
			boost::system::error_code& ec)
		{
			net::ssl::detail::io(next_layer_, core_, net::ssl::detail::handshake_op(type), ec);
			return ec;
		}

		/// Perform SSL handshaking.
		template <typename ConstBufferSequence>
		void handshake(handshake_type type, const ConstBufferSequence& buffers)
		{
			boost::system::error_code ec;
			handshake(type, buffers, ec);
			net::detail::throw_error(ec, "handshake");
		}

		/// Perform SSL handshaking.
		template <typename ConstBufferSequence>
		boost::system::error_code handshake(handshake_type type,
			const ConstBufferSequence& buffers, boost::system::error_code& ec)
		{
			net::ssl::detail::io(next_layer_, core_,
				net::ssl::detail::buffered_handshake_op<ConstBufferSequence>(type, buffers), ec);
			return ec;
		}

		/// Start an asynchronous SSL handshake.
		template <
			BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code))
				HandshakeToken = net::default_completion_token_t<executor_type>>
		auto async_handshake(handshake_type type,
			HandshakeToken&& token = net::default_completion_token_t<executor_type>())
			-> decltype(
			async_initiate<HandshakeToken,
				void (boost::system::error_code)>(
					net::declval<initiate_async_handshake>(), token, type))
		{
			return async_initiate<HandshakeToken,
			void (boost::system::error_code)>(
				initiate_async_handshake(this), token, type);
		}

		/// Start an asynchronous SSL handshake.
		template <typename ConstBufferSequence,
			BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
				std::size_t)) BufferedHandshakeToken
				= net::default_completion_token_t<executor_type>>
		auto async_handshake(handshake_type type, const ConstBufferSequence& buffers,
			BufferedHandshakeToken&& token
				= net::default_completion_token_t<executor_type>(),
			net::constraint_t<
				net::is_const_buffer_sequence<ConstBufferSequence>::value
			> = 0)
			-> decltype(
			async_initiate<BufferedHandshakeToken,
				void (boost::system::error_code, std::size_t)>(
					net::declval<initiate_async_buffered_handshake>(), token, type, buffers))
		{
			return async_initiate<BufferedHandshakeToken,
			void (boost::system::error_code, std::size_t)>(
				initiate_async_buffered_handshake(this), token, type, buffers);
		}

		/// Shut down SSL on the stream.
		void shutdown()
		{
			boost::system::error_code ec;
			shutdown(ec);
			net::detail::throw_error(ec, "shutdown");
		}

		/// Shut down SSL on the stream.
		boost::system::error_code shutdown(boost::system::error_code& ec)
		{
			net::ssl::detail::io(next_layer_, core_, net::ssl::detail::shutdown_op(), ec);
			return ec;
		}

		/// Asynchronously shut down SSL on the stream.
		template <
			BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code))
				ShutdownToken
				= net::default_completion_token_t<executor_type>>
		auto async_shutdown(
			ShutdownToken&& token = net::default_completion_token_t<executor_type>())
			-> decltype(
			async_initiate<ShutdownToken,
				void (boost::system::error_code)>(
				net::declval<initiate_async_shutdown>(), token))
		{
			return async_initiate<ShutdownToken,
			void (boost::system::error_code)>(
				initiate_async_shutdown(this), token);
		}

		/// Write some data to the stream.
		template <typename ConstBufferSequence>
		std::size_t write_some(const ConstBufferSequence& buffers)
		{
			boost::system::error_code ec;
			std::size_t n = write_some(buffers, ec);
			net::detail::throw_error(ec, "write_some");
			return n;
		}

		/// Write some data to the stream.
		template <typename ConstBufferSequence>
		std::size_t write_some(const ConstBufferSequence& buffers,
			boost::system::error_code& ec)
		{
			return net::ssl::detail::io(next_layer_, core_,
				net::ssl::detail::write_op<ConstBufferSequence>(buffers), ec);
		}

		/// Start an asynchronous write.
		template <typename ConstBufferSequence,
			BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
				std::size_t)) WriteToken = net::default_completion_token_t<executor_type>>
		auto async_write_some(const ConstBufferSequence& buffers,
			WriteToken&& token = net::default_completion_token_t<executor_type>())
			-> decltype(
			async_initiate<WriteToken,
				void (boost::system::error_code, std::size_t)>(
					net::declval<initiate_async_write_some>(), token, buffers))
		{
			return async_initiate<WriteToken,
			void (boost::system::error_code, std::size_t)>(
				initiate_async_write_some(this), token, buffers);
		}

		/// Read some data from the stream.
		template <typename MutableBufferSequence>
		std::size_t read_some(const MutableBufferSequence& buffers)
		{
			boost::system::error_code ec;
			std::size_t n = read_some(buffers, ec);
			net::detail::throw_error(ec, "read_some");
			return n;
		}

		/// Read some data from the stream.
		template <typename MutableBufferSequence>
		std::size_t read_some(const MutableBufferSequence& buffers,
			boost::system::error_code& ec)
		{
			return net::ssl::detail::io(next_layer_, core_,
				net::ssl::detail::read_op<MutableBufferSequence>(buffers), ec);
		}

		/// Start an asynchronous read.
		template <typename MutableBufferSequence,
			BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
				std::size_t)) ReadToken = net::default_completion_token_t<executor_type>>
		auto async_read_some(const MutableBufferSequence& buffers,
			ReadToken&& token = net::default_completion_token_t<executor_type>())
			-> decltype(
			async_initiate<ReadToken,
				void (boost::system::error_code, std::size_t)>(
					net::declval<initiate_async_read_some>(), token, buffers))
		{
			return async_initiate<ReadToken,
			void (boost::system::error_code, std::size_t)>(
				initiate_async_read_some(this), token, buffers);
		}

		private:
		class initiate_async_handshake
		{
		public:
			typedef typename ssl_stream::executor_type executor_type;

			explicit initiate_async_handshake(ssl_stream* self)
				: self_(self)
			{
			}

			executor_type get_executor() const noexcept
			{
				return self_->get_executor();
			}

			template <typename HandshakeHandler>
			void operator()(HandshakeHandler&& handler,
				handshake_type type) const
			{
				// If you get an error on the following line it means that your handler
				// does not meet the documented type requirements for a HandshakeHandler.
				BOOST_ASIO_HANDSHAKE_HANDLER_CHECK(HandshakeHandler, handler) type_check;

				net::detail::non_const_lvalue<HandshakeHandler> handler2(handler);
				net::ssl::detail::async_io(self_->next_layer_, self_->core_,
					net::ssl::detail::handshake_op(type), handler2.value);
			}

		private:
			ssl_stream* self_;
		};

		class initiate_async_buffered_handshake
		{
		public:
			typedef typename ssl_stream::executor_type executor_type;

			explicit initiate_async_buffered_handshake(ssl_stream* self)
				: self_(self)
			{
			}

			executor_type get_executor() const noexcept
			{
				return self_->get_executor();
			}

			template <typename BufferedHandshakeHandler, typename ConstBufferSequence>
			void operator()(BufferedHandshakeHandler&& handler,
				handshake_type type, const ConstBufferSequence& buffers) const
			{
				// If you get an error on the following line it means that your
				// handler does not meet the documented type requirements for a
				// BufferedHandshakeHandler.
				BOOST_ASIO_BUFFERED_HANDSHAKE_HANDLER_CHECK(
					BufferedHandshakeHandler, handler) type_check;

				net::detail::non_const_lvalue<
					BufferedHandshakeHandler> handler2(handler);
				net::ssl::detail::async_io(self_->next_layer_, self_->core_,
					net::ssl::detail::buffered_handshake_op<ConstBufferSequence>(type, buffers),
					handler2.value);
			}

		private:
			ssl_stream* self_;
		};

		class initiate_async_shutdown
		{
		public:
			typedef typename ssl_stream::executor_type executor_type;

			explicit initiate_async_shutdown(ssl_stream* self)
				: self_(self)
			{
			}

			executor_type get_executor() const noexcept
			{
				return self_->get_executor();
			}

			template <typename ShutdownHandler>
			void operator()(ShutdownHandler&& handler) const
			{
				// If you get an error on the following line it means that your handler
				// does not meet the documented type requirements for a ShutdownHandler.
				BOOST_ASIO_HANDSHAKE_HANDLER_CHECK(ShutdownHandler, handler) type_check;

				net::detail::non_const_lvalue<ShutdownHandler> handler2(handler);
				net::ssl::detail::async_io(self_->next_layer_, self_->core_,
					net::ssl::detail::shutdown_op(), handler2.value);
			}

		private:
			ssl_stream* self_;
		};

		class initiate_async_write_some
		{
		public:
			typedef typename ssl_stream::executor_type executor_type;

			explicit initiate_async_write_some(ssl_stream* self)
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
				// If you get an error on the following line it means that your handler
				// does not meet the documented type requirements for a WriteHandler.
				BOOST_ASIO_WRITE_HANDLER_CHECK(WriteHandler, handler) type_check;

				net::detail::non_const_lvalue<WriteHandler> handler2(handler);
				net::ssl::detail::async_io(self_->next_layer_, self_->core_,
					net::ssl::detail::write_op<ConstBufferSequence>(buffers), handler2.value);
			}

		private:
			ssl_stream* self_;
		};

		class initiate_async_read_some
		{
		public:
			typedef typename ssl_stream::executor_type executor_type;

			explicit initiate_async_read_some(ssl_stream* self)
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
				// If you get an error on the following line it means that your handler
				// does not meet the documented type requirements for a ReadHandler.
				BOOST_ASIO_READ_HANDLER_CHECK(ReadHandler, handler) type_check;

				net::detail::non_const_lvalue<ReadHandler> handler2(handler);
				net::ssl::detail::async_io(self_->next_layer_, self_->core_,
					net::ssl::detail::read_op<MutableBufferSequence>(buffers), handler2.value);
			}

		private:
			ssl_stream* self_;
		};

		Stream next_layer_;
		net::ssl::detail::stream_core core_;
	};
}

#endif // INCLUDE__2025_12_12__SSL_STREAM_HPP

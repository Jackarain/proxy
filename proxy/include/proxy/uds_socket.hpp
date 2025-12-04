//
// Copyright (C) 2025 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#ifndef INCLUDE__205_12_04___UDS_SOCKET_HPP
#define INCLUDE__205_12_04___UDS_SOCKET_HPP

#include <boost/beast/core/stream_traits.hpp>

#include <boost/beast/core/tcp_stream.hpp>

#include <boost/asio/executor.hpp>
#include <boost/asio/local/stream_protocol.hpp>

#include <memory>


namespace util {

	namespace net = boost::asio;
	using tcp = net::ip::tcp;               // from <boost/asio/ip/tcp.hpp>


	class uds_socket
	{
		uds_socket(const uds_socket&) = delete;
		uds_socket& operator=(uds_socket const&) = delete;

		using uds_tcp_stream = boost::beast::basic_stream<
			net::local::stream_protocol,
			net::any_io_executor,
			boost::beast::simple_rate_policy>;

	public:
		using next_layer_type = uds_tcp_stream;
		using executor_type = next_layer_type::executor_type;

		using lowest_layer_type = typename next_layer_type::socket_type;
		using endpoint_type = typename next_layer_type::endpoint_type;
		using protocol_type = typename next_layer_type::protocol_type;

		using native_handle_type = typename lowest_layer_type::native_handle_type;

	public:
		explicit uds_socket(net::local::stream_protocol::socket&& s)
			: impl_(std::make_unique<next_layer_type>(std::move(s)))
		{}

		explicit uds_socket(executor_type exec)
			: impl_(std::make_unique<next_layer_type>(exec))
		{}

		explicit uds_socket(net::io_context& ioc)
			: impl_(std::make_unique<next_layer_type>(ioc))
		{}

		uds_socket& operator=(uds_socket&& other) noexcept
		{
			if (this != &other)
				impl_ = std::move(other.impl_);
			return *this;
		}

		uds_socket(uds_socket&& other) noexcept
			: impl_(std::move(other.impl_))
		{}

		~uds_socket() = default;

	public:
		executor_type get_executor() const noexcept
		{
			return impl_->get_executor();
		}

		lowest_layer_type& lowest_layer() noexcept
		{
			return impl_->socket();
		}

		lowest_layer_type const& lowest_layer() const noexcept
		{
			return impl_->socket();
		}

		bool is_open() const noexcept
		{
			return lowest_layer().is_open();
		}

		void close()
		{
			impl_->close();
		}

		void close(boost::system::error_code& ec)
		{
			ec = {};
			impl_->close();
		}

		template <typename SettableSocketOption>
		void set_option(const SettableSocketOption& option)
		{
			lowest_layer().set_option(option);
		}

		template <typename SettableSocketOption>
		void set_option(const SettableSocketOption& option,
			boost::system::error_code& ec)
		{
			lowest_layer().set_option(option, ec);
		}

		template <typename WaitHandler>
		auto async_wait(net::socket_base::wait_type w, WaitHandler&& handler)
		{
			return impl_->socket().async_wait(w, std::forward<WaitHandler>(handler));
		}

		template <typename ConnectHandler>
		auto async_connect(const tcp::endpoint& endpoint, ConnectHandler&& handler)
		{
			return impl_->async_connect(endpoint,
				std::forward<ConnectHandler>(handler));
		}

		template <typename MutableBufferSequence, typename ReadHandler>
		auto async_read_some(const MutableBufferSequence& buffers, ReadHandler&& handler)
		{
			return impl_->async_read_some(buffers, std::forward<ReadHandler>(handler));
		}

		template <typename ConstBufferSequence, typename WriteHandler>
		auto async_write_some(const ConstBufferSequence& buffers, WriteHandler&& handler)
		{
			return impl_->async_write_some(buffers, std::forward<WriteHandler>(handler));
		}

		void expires_after(net::steady_timer::duration expiry_time)
		{
			impl_->expires_after(expiry_time);
		}

		void expires_at(net::steady_timer::time_point expiry_time)
		{
			impl_->expires_at(expiry_time);
		}

		void expires_never()
		{
			impl_->expires_never();
		}

		void rate_limit(int bytes_per_second) noexcept
		{
			auto& policy = impl_->rate_policy();

			std::size_t rate = bytes_per_second;

			if (bytes_per_second < 0)
				rate = (std::numeric_limits<std::size_t>::max)();

			policy.read_limit(rate);
			policy.write_limit(rate);
		}

	private:
		std::unique_ptr<next_layer_type> impl_;
	};
}

#endif // INCLUDE__205_12_04___UDS_SOCKET_HPP

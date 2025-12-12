//
// Copyright (C) 2024 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#ifndef INCLUDE__2025_12_12__STDIO_STREAM_HPP
#define INCLUDE__2025_12_12__STDIO_STREAM_HPP

#if defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
# include <boost/asio/posix/stream_descriptor.hpp>
#else
# include <boost/asio/windows/stream_handle.hpp>
#endif // BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR

#include <boost/asio/executor.hpp>

#include <memory>


namespace util {

	namespace net = boost::asio;


	class stdio_stream
	{
		stdio_stream(const stdio_stream&) = delete;
		stdio_stream& operator=(stdio_stream const&) = delete;

	public:
#if defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
		using next_layer_type = net::posix::stream_descriptor;
#else
		using next_layer_type = net::windows::stream_handle;
#endif

		using executor_type = next_layer_type::executor_type;
		using lowest_layer_type = typename next_layer_type::lowest_layer_type;

		using endpoint_type = std::string;

		using native_handle_type = typename lowest_layer_type::native_handle_type;

	public:
		explicit stdio_stream(next_layer_type&& s)
			: impl_(std::make_unique<next_layer_type>(std::move(s)))
		{}

		explicit stdio_stream(executor_type exec)
			: impl_(std::make_unique<next_layer_type>(exec))
		{}

		explicit stdio_stream(net::io_context& ioc)
			: impl_(std::make_unique<next_layer_type>(ioc))
		{}

		stdio_stream& operator=(stdio_stream&& other) noexcept
		{
			if (this != &other)
				impl_ = std::move(other.impl_);
			return *this;
		}

		stdio_stream(stdio_stream&& other) noexcept
			: impl_(std::move(other.impl_))
		{}

		~stdio_stream() = default;

	public:
		executor_type get_executor() const noexcept
		{
			return impl_->get_executor();
		}

		lowest_layer_type& lowest_layer() noexcept
		{
			return *impl_;
		}

		lowest_layer_type const& lowest_layer() const noexcept
		{
			return *impl_;
		}

		endpoint_type remote_endpoint() const
		{
			return {};
		}

		endpoint_type local_endpoint() const
		{
			return {};
		}

		bool is_open() const noexcept
		{
			return impl_->is_open();
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

	private:
		std::unique_ptr<next_layer_type> impl_;
	};
}

#endif // INCLUDE__2025_12_12__STDIO_STREAM_HPP

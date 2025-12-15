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
#endif // BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR

#include <boost/asio/executor.hpp>

#include <memory>


namespace util {

	namespace net = boost::asio;

#if !defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
	class stdio_stream
	{
		stdio_stream(const stdio_stream&) = delete;
		stdio_stream& operator=(stdio_stream const&) = delete;

	public:
		using executor_type = net::any_io_executor;
		using lowest_layer_type = stdio_stream;
		using native_handle_type = void*;

		stdio_stream(executor_type in_executor, executor_type out_executor)
			: in_executor_(in_executor)
			, out_executor_(out_executor)
		{
			// std::cin.sync_with_stdio(false);
			// std::cout.sync_with_stdio(false);
		}

		stdio_stream& operator=(stdio_stream&& other) noexcept
		{
			if (this != &other)
			{
				in_executor_ = other.in_executor_;
				out_executor_ = other.out_executor_;
			}
			return *this;
		}

		stdio_stream(stdio_stream&& other) noexcept
			: in_executor_(other.in_executor_)
			, out_executor_(other.out_executor_)
		{
		}

		executor_type get_executor() noexcept
		{
			return out_executor_;
		}

		executor_type get_in_executor() noexcept
		{
			return in_executor_;
		}

		executor_type get_out_executor() noexcept
		{
			return out_executor_;
		}

		lowest_layer_type& lowest_layer() noexcept
		{
			return *this;
		}

		lowest_layer_type const& lowest_layer() const noexcept
		{
			return *this;
		}

		bool is_open() const noexcept
		{
			return true;
		}

		void close()
		{
		}

		void close(boost::system::error_code ec)
		{
			ec = {};
		}

		class initiate_async_read_some
		{
		public:
			using executor_type = typename stdio_stream::executor_type;

			explicit initiate_async_read_some(stdio_stream* self)
				: self_(self)
			{
			}

			template <typename ReadHandler, typename MutableBufferSequence>
			void operator()(ReadHandler&& handler, const MutableBufferSequence& buffers) const
			{
				net::mutable_buffer buffer =
					net::detail::buffer_sequence_adapter<
					net::mutable_buffer, MutableBufferSequence>::first(buffers);

				DWORD bytes_read = 0;
				static HANDLE input_handle(::GetStdHandle(STD_INPUT_HANDLE));

				::ReadFile(input_handle, buffer.data(), static_cast<DWORD>(buffer.size()), &bytes_read, NULL);

				boost::system::error_code ec;
				handler(ec, bytes_read);
			}

		private:
			stdio_stream* self_;
		};

		class initiate_async_write_some
		{
		public:
			using executor_type = typename stdio_stream::executor_type;

			explicit initiate_async_write_some(stdio_stream* self)
				: self_(self)
			{
			}

			template <typename WriteHandler, typename ConstBufferSequence>
			void operator()(WriteHandler&& handler, const ConstBufferSequence& buffers) const
			{
				net::const_buffer buffer =
					net::detail::buffer_sequence_adapter<
					net::const_buffer, ConstBufferSequence>::first(buffers);

				DWORD bytes_written = 0;
				static HANDLE output_handle(::GetStdHandle(STD_OUTPUT_HANDLE));

				::WriteFile(output_handle, buffer.data(), static_cast<DWORD>(buffer.size()), &bytes_written, NULL);

				boost::system::error_code ec;
				handler(ec, bytes_written);
			}

		private:
			stdio_stream* self_;
		};


		template <typename MutableBufferSequence, typename ReadHandler>
		auto async_read_some(const MutableBufferSequence& buffers, ReadHandler&& handler)
		{
			return net::async_initiate<ReadHandler,
				void(boost::system::error_code, std::size_t)>(
					initiate_async_read_some(this), handler, buffers);
		}

		template <typename ConstBufferSequence, typename WriteHandler>
		auto async_write_some(const ConstBufferSequence& buffers, WriteHandler&& handler)
		{
			return net::async_initiate<WriteHandler,
				void(boost::system::error_code, std::size_t)>(
					initiate_async_write_some(this), handler, buffers);
		}

	private:
		executor_type out_executor_;
		executor_type in_executor_;
	};

#else

	class stdio_stream
	{
		stdio_stream(const stdio_stream&) = delete;
		stdio_stream& operator=(stdio_stream const&) = delete;

	public:
		using next_layer_type = net::posix::stream_descriptor;

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

		executor_type get_executor() noexcept
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
			impl_->close(ec);
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

#endif

}

#endif // INCLUDE__2025_12_12__STDIO_STREAM_HPP

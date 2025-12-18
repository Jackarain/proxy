//
// Copyright (C) 2019 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#ifndef INCLUDE__2023_10_18__FILEOP_HPP
#define INCLUDE__2023_10_18__FILEOP_HPP


#include <boost/filesystem.hpp>
#include <boost/nowide/fstream.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/system.hpp>

#include <cstdint>
#include <span>
#include <streambuf>
#include <string>
#include <string_view>
#include <sstream>
#include <type_traits>

#include <openssl/evp.h>

//////////////////////////////////////////////////////////////////////////

namespace fileop {

	namespace fs = boost::filesystem;
	namespace net = boost::asio;

	template<typename T>
	concept ByteType = std::same_as<T, uint8_t> || std::same_as<T, char>;

	namespace details {

		// 创建目录, 包含创建子目录.
		// 如果目录存在或被成功创建则返回 true, 否则返回 false.
		inline bool create_parent_directories(const fs::path& p)
		{
			try
			{
				if (exists(p))
					return true;

				create_directories(p.parent_path());
			}
			catch (const std::exception&)
			{}

			return false;
		}

		template<ByteType T>
		std::streamsize write(std::streambuf& buf, std::span<T> val)
		{
			auto bytes = val.size();
			return buf.sputn(reinterpret_cast<const char*>(val.data()), bytes);
		}

		inline std::streamsize
		write(std::streambuf& buf, std::string_view val)
		{
			auto bytes = val.size();
			return buf.sputn(reinterpret_cast<const char*>(val.data()), bytes);
		}


		template<ByteType T>
		std::streamsize read(std::streambuf& buf, std::span<T> val)
		{
			auto bytes = val.size();
			return buf.sgetn(reinterpret_cast<char*>(val.data()), bytes);
		}

		inline std::streamsize
		read(std::streambuf& buf, std::string_view val)
		{
			std::streamsize bytes = val.size();
			return buf.sgetn((char*)(val.data()), bytes);
		}

		inline std::streamsize
		filesize(const fs::path& path)
		{
			try
			{
				return file_size(path);
			}
			catch (const std::exception&)
			{}

			return -1;
		}

		//////////////////////////////////////////////////////////////////////////

		template <typename P>
		inline std::string
		file_hash_impl(const fs::path& p, boost::system::error_code& ec, P cancel_op) noexcept
		{
			ec = {};

			boost::nowide::fstream file(p, std::ios::in | std::ios::binary);
			if (!file)
			{
				ec = boost::system::error_code(errno, boost::system::generic_category());
				return {};
			}

			EVP_MD_CTX* ctx = EVP_MD_CTX_new();
			if (!ctx)
			{
				ec = boost::system::errc::make_error_code(boost::system::errc::invalid_argument);
				return {};
			}

			struct ctx_guard
			{
				EVP_MD_CTX* ctx;
				~ctx_guard() { EVP_MD_CTX_free(ctx); }
			} guard { ctx };

			const EVP_MD* md = EVP_sha1();
			if (!md)
			{
				ec = boost::system::errc::make_error_code(boost::system::errc::invalid_argument);
				return {};
			}

			if (EVP_DigestInit_ex(ctx, md, nullptr) != 1)
			{
				ec = boost::system::errc::make_error_code(boost::system::errc::io_error);
				return {};
			}

			const auto buf_size = 1024 * 1024 * 4;
			auto bufs = std::make_unique_for_overwrite<char[]>(buf_size);

			while (file.read(bufs.get(),buf_size) || file.gcount() > 0)
			{
				if (cancel_op())
				{
					ec = net::error::operation_aborted;
					return {};
				}

				const auto n = static_cast<std::size_t>(file.gcount());

				if (EVP_DigestUpdate(ctx, bufs.get(), n) != 1)
				{
					ec = boost::system::errc::make_error_code(boost::system::errc::io_error);
					return {};
				}
			}

			unsigned char digest[EVP_MAX_MD_SIZE];
			unsigned int digest_len = 0;

			if (EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1)
			{
				ec = boost::system::errc::make_error_code(boost::system::errc::io_error);
				return {};
			}

			std::stringstream ss;

			ss << std::hex << std::setfill('0');
			for (unsigned int i = 0; i < digest_len; ++i)
				ss << std::setw(2) << static_cast<unsigned int>(digest[i]);

			return ss.str();
		}
	}

	//////////////////////////////////////////////////////////////////////////

	template<ByteType T>
	std::streamsize read(const std::streambuf& buf, std::span<T> val)
	{
		return details::read(buf, val);
	}

	inline std::streamsize
	read(std::streambuf& buf, std::string& val)
	{
		std::streamsize bytes = val.size();
		return buf.sgetn(reinterpret_cast<char*>(val.data()), bytes);
	}


	template<ByteType T>
	std::streamsize read(const boost::nowide::fstream& file, std::span<T> val)
	{
		return details::read(*file.rdbuf(), val);
	}

	inline std::streamsize
	read(const boost::nowide::fstream& file, std::string& val)
	{
		return details::read(*file.rdbuf(), val);
	}

	template<ByteType T>
	std::streamsize read(const fs::path& file, std::span<T> val)
	{
		boost::nowide::fstream f(file, std::ios_base::binary | std::ios_base::in);
		return details::read(*f.rdbuf(), val);
	}

	inline std::streamsize
	read(const fs::path& file, std::string& val)
	{
		boost::nowide::fstream f(file, std::ios_base::binary | std::ios_base::in);
		std::error_code ec;
		auto fsize = details::filesize(file);
		if (fsize < 0)
			return -1;
		val.resize(fsize);
		return details::read<char>(*f.rdbuf(), val);
	}

	//////////////////////////////////////////////////////////////////////////


	template<ByteType T>
	std::streamsize write(std::streambuf& buf, std::span<T> val)
	{
		return details::write(buf, val);
	}

	inline std::streamsize
	write(std::streambuf& buf, std::string_view val)
	{
		return details::write(buf, val);
	}


	template<ByteType T>
	std::streamsize write(boost::nowide::fstream& file, std::span<T> val)
	{
		return details::write(*file.rdbuf(), val);
	}

	inline std::streamsize
	write(boost::nowide::fstream& file, std::string_view val)
	{
		return details::write(*file.rdbuf(), val);
	}


	template<ByteType T>
	std::streamsize write(const fs::path& file, std::span<T> val)
	{
		details::create_parent_directories(file);

		boost::nowide::fstream f(file,
			std::ios_base::binary |
			std::ios_base::out |
			std::ios_base::trunc);

		return details::write(*f.rdbuf(), val);
	}

	inline std::streamsize
	write(const fs::path& file, std::string_view val)
	{
		details::create_parent_directories(file);

		boost::nowide::fstream f(file,
			std::ios_base::binary |
			std::ios_base::out |
			std::ios_base::trunc);

		return details::write(*f.rdbuf(), val);
	}


	//////////////////////////////////////////////////////////////////////////


	inline std::string
	file_hash(const fs::path& p, boost::system::error_code& ec) noexcept
	{
		return details::file_hash_impl(p, ec, []() { return false; });
	}

	template <typename CompletionToken>
	auto async_hash_file(const fs::path& path, CompletionToken&& token) noexcept
	{
		return net::async_initiate<CompletionToken,
			void(boost::system::error_code, std::string)>
			([path](auto&& handler) mutable
				{
					std::thread([path = std::move(path), handler = std::move(handler)]() mutable
						{
							auto slot = net::get_associated_cancellation_slot(handler);
							auto executor = net::get_associated_executor(handler);
							std::atomic_bool cancelled = false;

							if (slot.is_connected())
							{
								slot.assign([&cancelled](net::cancellation_type_t) mutable
									{
										cancelled = true;
									});
							}

							boost::system::error_code ec;
							auto hash = details::file_hash_impl(path, ec, [&cancelled]() -> bool { return cancelled; });

							net::post(executor,
								[ec = std::move(ec), hash = std::move(hash), handler = std::move(handler)]() mutable
								{
									handler(ec, hash);
								});
						}
					).detach();
				}, token);
	}

}

#endif // INCLUDE__2023_10_18__FILEOP_HPP

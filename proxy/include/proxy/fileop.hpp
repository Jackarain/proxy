//
// Copyright (C) 2019 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#ifndef INCLUDE__2023_10_18__FILEOP_HPP
#define INCLUDE__2023_10_18__FILEOP_HPP


#include <boost/filesystem.hpp>
#include <boost/uuid/detail/sha1.hpp>
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
		ec = {};

		boost::nowide::fstream file(p, std::ios::in | std::ios::binary);
		if (!file)
		{
			ec = boost::system::error_code(errno, boost::system::generic_category());
			return {};
		}

		boost::uuids::detail::sha1 sha1;
		const auto buf_size = 1024 * 1024 * 4;
		auto bufs = std::make_unique_for_overwrite<char[]>(buf_size);

		while (file.read(bufs.get(), buf_size) || file.gcount())
			sha1.process_bytes(bufs.get(), file.gcount());

		boost::uuids::detail::sha1::digest_type hash;
		sha1.get_digest(hash);

		std::stringstream ss;
		for (auto const& c : hash)
			ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(c);

		return ss.str();
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
							auto executor = net::get_associated_executor(handler);

							boost::system::error_code ec;
							auto hash = file_hash(path, ec);

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

//
// Copyright (C) 2019 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#ifndef INCLUDE__2023_10_18__FILEOP_HPP
#define INCLUDE__2023_10_18__FILEOP_HPP


#if __has_include(<boost/filesystem.hpp>)
#  define HAS_BOOST_FILESYSTEM
#  include <boost/filesystem.hpp>
#endif

#include <cstdint>
#include <fstream>
#include <span>
#include <streambuf>
#include <string>
#include <string_view>
#include <type_traits>
#include <filesystem>


//////////////////////////////////////////////////////////////////////////

namespace fileop {

	template<typename T>
	concept ByteType = std::same_as<T, uint8_t> || std::same_as<T, char>;

	template<typename P>
	concept PathType = std::same_as<std::decay_t<P>, std::filesystem::path>
#ifdef HAS_BOOST_FILESYSTEM
		|| std::same_as<std::decay_t<P>, boost::filesystem::path>;
#endif

	namespace details {

		// 创建目录, 包含创建子目录.
		// 如果目录存在或被成功创建则返回 true, 否则返回 false.
		template<typename P>
		inline bool create_parent_directories(const P& p)
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

		template<PathType P>
		std::streamsize filesize(const P& path)
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
	std::streamsize read(const std::fstream& file, std::span<T> val)
	{
		return details::read(*file.rdbuf(), val);
	}

	inline std::streamsize
	read(const std::fstream& file, std::string& val)
	{
		return details::read(*file.rdbuf(), val);
	}

	template<PathType P, ByteType T>
	std::streamsize read(const P& file, std::span<T> val)
	{
		std::fstream f(file.string(), std::ios_base::binary | std::ios_base::in);
		return details::read(*f.rdbuf(), val);
	}

	template<PathType P>
	std::streamsize read(const P& file, std::string& val)
	{
		std::fstream f(file.string(), std::ios_base::binary | std::ios_base::in);
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
	std::streamsize write(std::fstream& file, std::span<T> val)
	{
		return details::write(*file.rdbuf(), val);
	}

	inline std::streamsize
	write(std::fstream& file, std::string_view val)
	{
		return details::write(*file.rdbuf(), val);
	}


	template<PathType P, ByteType T>
	std::streamsize write(const P& file, std::span<T> val)
	{
		details::create_parent_directories(file);

		std::fstream f(file.string(),
			std::ios_base::binary |
			std::ios_base::out |
			std::ios_base::trunc);

		return details::write(*f.rdbuf(), val);
	}

	template<PathType P>
	std::streamsize write(const P& file, std::string_view val)
	{
		details::create_parent_directories(file);

		std::fstream f(file.string(),
			std::ios_base::binary |
			std::ios_base::out |
			std::ios_base::trunc);

		return details::write(*f.rdbuf(), val);
	}
}

#endif // INCLUDE__2023_10_18__FILEOP_HPP

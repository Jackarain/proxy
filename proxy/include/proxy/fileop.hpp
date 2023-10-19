//
// Copyright (C) 2019 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#ifndef INCLUDE__2023_10_18__FILEOP_HPP
#define INCLUDE__2023_10_18__FILEOP_HPP


#include <cstdint>
#include <fstream>
#include <streambuf>
#include <string>
#include <vector>
#include <string_view>
#include <filesystem>
#include <span>
#include <type_traits>


//////////////////////////////////////////////////////////////////////////

namespace fileop {
	namespace details {

		inline bool create_parent_directories(const std::filesystem::path& p)
		{
			std::error_code ec;

			if (std::filesystem::exists(p, ec))
				return true;

			if (ec)
				return false;

			if (!p.parent_path().empty())
			{
				std::filesystem::create_directories(p.parent_path(), ec);
				if (ec)
					return false;
			}

			return true;
		}

		template<class T>
		requires std::is_same_v<T, uint8_t> || std::is_same_v<T, char>
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


		template<class T>
		requires std::is_same_v<T, uint8_t> || std::is_same_v<T, char>
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
	}

	//////////////////////////////////////////////////////////////////////////

	template<class T>
	requires std::is_same_v<T, uint8_t> || std::is_same_v<T, char>
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


	template<class T>
	requires std::is_same_v<T, uint8_t> || std::is_same_v<T, char>
	std::streamsize read(const std::fstream& file, std::span<T> val)
	{
		return details::read(*file.rdbuf(), val);
	}

	inline std::streamsize
	read(const std::fstream& file, std::string& val)
	{
		return details::read(*file.rdbuf(), val);
	}


	template<class T>
	requires std::is_same_v<T, uint8_t> || std::is_same_v<T, char>
	std::streamsize read(const std::filesystem::path& file, std::span<T> val)
	{
		std::fstream f(file, std::ios_base::binary | std::ios_base::in);
		return details::read(*f.rdbuf(), val);
	}

	inline std::streamsize
	read(const std::filesystem::path& file, std::string& val)
	{
		std::fstream f(file, std::ios_base::binary | std::ios_base::in);
		std::error_code ec;
		auto fsize = std::filesystem::file_size(file, ec);
		if (ec)
			return -1;
		val.resize(fsize);
		return details::read<char>(*f.rdbuf(), val);
	}

	//////////////////////////////////////////////////////////////////////////


	template<class T>
	requires std::is_same_v<T, uint8_t> || std::is_same_v<T, char>
	std::streamsize write(std::streambuf& buf, std::span<T> val)
	{
		return details::write(buf, val);
	}

	inline std::streamsize
	write(std::streambuf& buf, std::string_view val)
	{
		return details::write(buf, val);
	}


	template<class T>
	requires std::is_same_v<T, uint8_t> || std::is_same_v<T, char>
	std::streamsize write(std::fstream& file, std::span<T> val)
	{
		return details::write(*file.rdbuf(), val);
	}

	inline std::streamsize
	write(std::fstream& file, std::string_view val)
	{
		return details::write(*file.rdbuf(), val);
	}


	template<class T>
	requires std::is_same_v<T, uint8_t> || std::is_same_v<T, char>
	std::streamsize write(const std::filesystem::path& file, std::span<T> val)
	{
		details::create_parent_directories(file);

		std::fstream f(file,
			std::ios_base::binary |
			std::ios_base::out |
			std::ios_base::trunc);

		return details::write(*f.rdbuf(), val);
	}

	inline std::streamsize
	write(const std::filesystem::path& file, std::string_view val)
	{
		details::create_parent_directories(file);

		std::fstream f(file,
			std::ios_base::binary |
			std::ios_base::out |
			std::ios_base::trunc);

		return details::write(*f.rdbuf(), val);
	}
}

#endif // INCLUDE__2023_10_18__FILEOP_HPP

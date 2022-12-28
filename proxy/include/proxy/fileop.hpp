//
// Copyright (C) 2019 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#pragma once

#include <streambuf>
#include <string>
#include <vector>
#include <string_view>
#include <filesystem>
#include <span>

namespace fileop {
	namespace details {

		inline void create_parent_directories(const std::filesystem::path& p)
		{
			std::error_code ec;
			auto e = std::filesystem::exists(p, ec);
			if (ec)
				return;

			if (!e)
			{
				if (!p.parent_path().empty())
				{
					std::filesystem::create_directories(
						p.parent_path(), ec);
				}
			}
		}

		template<class T>
		std::streamsize
		write(std::streambuf& buf, const T& val)
		{
			static_assert(std::is_standard_layout<T>{},
				"data is not standard layout");

			std::streamsize bytes = sizeof(T);
			return buf.sputn(reinterpret_cast<const char*>(&val), bytes);
		}

		inline std::streamsize
		write(std::streambuf& buf, const std::vector<uint8_t>& val)
		{
			std::streamsize bytes = val.size();
			return buf.sputn(reinterpret_cast<const char*>(val.data()), bytes);
		}

		inline std::streamsize
		write(std::streambuf& buf, const std::string& val)
		{
			std::streamsize bytes = val.size();
			return buf.sputn(reinterpret_cast<const char*>(val.data()), bytes);
		}

		inline std::streamsize
		write(std::streambuf& buf, const std::string_view& val)
		{
			std::streamsize bytes = val.size();
			return buf.sputn(reinterpret_cast<const char*>(val.data()), bytes);
		}

		inline std::streamsize
		write(std::streambuf& buf, const std::span<uint8_t>& val)
		{
			std::streamsize bytes = val.size();
			return buf.sputn(reinterpret_cast<const char*>(val.data()), bytes);
		}

		template<class T>
		std::streamsize
		read(std::streambuf& buf, T& val)
		{
			static_assert(std::is_standard_layout<T>{},
				"data is not standard layout");

			std::streamsize bytes = sizeof(T);
			return buf.sgetn(reinterpret_cast<char*>(&val), bytes);
		}

		inline std::streamsize
		read(std::streambuf& buf, std::vector<uint8_t>& val)
		{
			std::streamsize bytes = val.size();
			return buf.sgetn(reinterpret_cast<char*>(val.data()), bytes);
		}

		inline std::streamsize
		read(std::streambuf& buf, std::string& val)
		{
			std::streamsize bytes = val.size();
			return buf.sgetn(reinterpret_cast<char*>(val.data()), bytes);
		}

		inline std::streamsize
		read(std::streambuf& buf, std::string_view& val)
		{
			std::streamsize bytes = val.size();
			return buf.sgetn((char*)(val.data()), bytes);
		}

		inline std::streamsize
		read(std::streambuf& buf, std::span<uint8_t>& val)
		{
			std::streamsize bytes = val.size();
			return buf.sgetn((char*)(val.data()), bytes);
		}
	}

	template<class T>
	std::streamsize read(const std::streambuf& buf, T& val)
	{
		using details::read;
		return read(buf, val);
	}

	template<class T>
	std::streamsize write(const std::streambuf& buf, T const& val)
	{
		using details::write;
		return write(buf, val);
	}

	template<class T>
	std::streamsize read(const std::fstream& file, T& val)
	{
		using details::read;
		return read(*file.rdbuf(), val);
	}

	template<class T>
	std::streamsize write(const std::fstream& file, T const& val)
	{
		using details::write;
		return write(*file.rdbuf(), val);
	}

	template<class T>
	std::streamsize read(const std::filesystem::path& file, T& val)
	{
		std::fstream f(file, std::ios_base::binary | std::ios_base::in);
		auto fsize = std::filesystem::file_size(file);
		val.resize(fsize);

		using details::read;
		return read(*f.rdbuf(), val);
	}

	template<class T>
	std::streamsize write(const std::filesystem::path& file, T const& val)
	{
		using details::create_parent_directories;
		create_parent_directories(file);

		std::fstream f(file, std::ios_base::binary |
			std::ios_base::out |
			std::ios_base::trunc);

		using details::write;
		return write(*f.rdbuf(), val);
	}
}

//
// socks_io.hpp
// ~~~~~~~~~~~~
//
// Copyright (c) 2019 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2023_10_18__SOCKS_IO_HPP
#define INCLUDE__2023_10_18__SOCKS_IO_HPP


#include <cstddef>


namespace io_util {

	template<typename type, typename source>
	type read(source& p)
	{
		type ret = 0;
		for (std::size_t i = 0; i < sizeof(type); i++)
			ret = (ret << 8) | (static_cast<unsigned char>(*p++));
		return ret;
	}

	template<typename type, typename target>
	void write(type v, target& p)
	{
		for (auto i = (int)sizeof(type) - 1; i >= 0; i--, p++)
			*p = static_cast<unsigned char>((v >> (i * 8)) & 0xff);
	}

}

#endif // INCLUDE__2023_10_18__SOCKS_IO_HPP

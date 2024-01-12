//
// Copyright (C) 2019 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#ifndef INCLUDE__2019_10_18__FILEOP_HPP
#define INCLUDE__2019_10_18__FILEOP_HPP

#include "boost/asio/ip/address.hpp"
#include <vector>
#include <string>
#include <exception>

#include <boost/filesystem.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/address_v6.hpp>

namespace proxy {
	namespace net = boost::asio;

	class ipip
	{
	public:
		virtual ~ipip() = default;

		virtual bool load(const std::string& filename) = 0;

		virtual std::tuple<std::vector<std::string>, std::string>
		lookup(net::ip::address ip) = 0;
	};

	class ipip_datx : public ipip
	{
		// c++11 noncopyable.
		ipip_datx(const ipip_datx&) = delete;
		ipip_datx& operator=(const ipip_datx&) = delete;

	public:
		ipip_datx() = default;
		virtual ~ipip_datx() = default;

		// 打开ipip数据文件.
		virtual bool load(const std::string& filename) override;

		// 根据一个IP查询对应地址信息.
		virtual std::tuple<std::vector<std::string>, std::string>
		lookup(net::ip::address ip) override;

	private:
		std::vector<unsigned char> m_data;
		std::vector<unsigned char> m_index;
		std::vector<uint32_t> m_flag;
		unsigned m_offset;
	};

}

#endif // INCLUDE__2019_10_18__FILEOP_HPP
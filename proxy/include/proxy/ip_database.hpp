//
// Copyright (C) 2019 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#ifndef INCLUDE__2019_10_18__IPIP_HPP
#define INCLUDE__2019_10_18__IPIP_HPP

#include <boost/filesystem.hpp>
#include <boost/asio/ip/address.hpp>

#include <vector>
#include <map>
#include <string>
#include <exception>


namespace proxy {
	namespace net = boost::asio;

	class ip_database
	{
	public:
		virtual ~ip_database() = default;

		virtual bool load(const std::string& filename) = 0;

		// 根据一个IP查询对应地址信息.
		// 返回值: <地址信息, 运营商>, 其中 std::vector<std::string> 为地址
		// 信息, 按地区从大到小排列.
		// 如: ["中国", "北京", "北京", "联通"], "联通" 为运营商.
		virtual std::tuple<std::vector<std::string>, std::string>
		lookup(net::ip::address ip) = 0;
	};

	// IPIP数据文件格式: datx.
	class ip_datx : public ip_database
	{
		// c++11 noncopyable.
		ip_datx(const ip_datx&) = delete;
		ip_datx& operator=(const ip_datx&) = delete;

	public:
		ip_datx() = default;
		virtual ~ip_datx() = default;

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

	// IPDB 数据文件格式: db.
	class ip_ipdb : public ip_database
	{
		// c++11 noncopyable.
		ip_ipdb(const ip_ipdb&) = delete;
		ip_ipdb& operator=(const ip_ipdb&) = delete;

	public:
		ip_ipdb() = default;
		virtual ~ip_ipdb() = default;

		// 打开ipip数据文件.
		virtual bool load(const std::string& filename) override;

		// 根据一个IP查询对应地址信息.
		virtual std::tuple<std::vector<std::string>, std::string>
		lookup(net::ip::address ip) override;

	private:
		void parse_meta(const std::string& json);
		int read_node(int node, int bit) const;
		int search_tree(const uint8_t* ip, int bits, int startNode) const;
		std::string resolve_content(int node) const;
		int guess_isp_index();

	private:
		bool loaded_ = false;
		std::vector<uint8_t> data_;

		// 元数据信息
		int nodeCount_ = 0;
		int totalSize_ = 0;
		uint16_t ipVersion_ = 0;
		std::vector<std::string> fieldNames_;
		std::map<std::string, int> languages_;

		// 运行时属性
		int v4offset_ = 0;
		int ispIdx_ = -1;
		std::string currentLang_;
	};
}

#endif // INCLUDE__2019_10_18__IPIP_HPP
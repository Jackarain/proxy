//
// Copyright (C) 2019 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#ifndef INCLUDE__2019_10_18__IP_DATABASE_HPP
#define INCLUDE__2019_10_18__IP_DATABASE_HPP

#include <boost/filesystem.hpp>
#include <boost/asio/ip/address.hpp>

#include <vector>
#include <map>
#include <string>


namespace proxy {
	namespace net = boost::asio;

	struct ip_result
	{
		std::vector<std::string> regions;
		std::string isp;
	};

	class ip_database
	{
	public:
		virtual ~ip_database() = default;

		virtual bool load(const std::string& filename) = 0;

		// 根据一个IP查询对应地址信息.
		// 返回值: <地址信息, 运营商>, 其中 std::vector<std::string> 为地址
		// 信息, 按地区从大到小排列.
		// 如: ["中国", "北京", "北京", "联通"], "联通" 为运营商.
		virtual ip_result lookup(net::ip::address ip) = 0;
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
		bool load(const std::string& filename) override;

		// 根据一个IP查询对应地址信息.
		ip_result lookup(net::ip::address ip) override;

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
		bool load(const std::string& filename) override;

		// 根据一个IP查询对应地址信息.
		ip_result lookup(net::ip::address ip) override;

	private:
		void parse_meta(const std::string& json);
		int read_node(int node, int bit) const;
		int search_tree(const uint8_t* ip, int bits, int startNode) const;
		std::string resolve_content(int node) const;
		int guess_isp_index();

	private:
		bool m_loaded = false;
		std::vector<uint8_t> m_data;

		// 元数据信息
		int m_node_count = 0;
		int m_total_size = 0;
		uint16_t m_ip_version = 0;
		std::vector<std::string> m_field_names;
		std::map<std::string, int> m_languages;

		// 运行时属性
		int m_v4offset = 0;
		int m_isp_idx = -1;
		std::string m_current_lang;
	};
}

#endif // INCLUDE__2019_10_18__IP_DATABASE_HPP

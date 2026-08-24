//
// dns_server.cpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/dns_server.hpp"
#include "proxy/proxy_util.hpp"
#include "proxy/socks_io.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/json.hpp>

#include <openssl/ssl.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>


namespace proxy {

	using io_util::read;
	using io_util::write;

	// UDP DNS 在途查询协程数上限，超过直接丢弃（UDP 无连接，丢包由客户端
	// 自行重试）.
	inline constexpr int k_dns_max_inflight = 256;

	// 本地解析应答的 TTL（系统解析不暴露原始 TTL，统一使用该值）.
	static constexpr uint32_t dns_local_ttl = 60;

	//////////////////////////////////////////////////////////////////////////
	// DNS wire-format 工具函数实现（与 proxy_session 的 HTTP DoH 路径共享）.

	// 统一的 DNS 类型名称 <-> 数值映射表, 避免两份重复数据.
	static const std::unordered_map<uint16_t, std::string>& dns_type_map()
	{
		static const std::unordered_map<uint16_t, std::string> type_map = {
			{1, "A"}, {2, "NS"}, {3, "MD"}, {4, "MF"}, {5, "CNAME"},
			{6, "SOA"}, {7, "MB"}, {8, "MG"}, {9, "MR"}, {10, "NULL"},
			{11, "WKS"}, {12, "PTR"}, {13, "HINFO"}, {14, "MINFO"}, {15, "MX"},
			{16, "TXT"}, {17, "RP"}, {18, "AFSDB"}, {19, "X25"}, {20, "ISDN"},
			{21, "RT"}, {22, "NSAP"}, {23, "NSAP-PTR"}, {24, "SIG"}, {25, "KEY"},
			{26, "PX"}, {27, "GPOS"}, {28, "AAAA"}, {29, "LOC"}, {30, "NXT"},
			{33, "SRV"}, {35, "NAPTR"}, {36, "KX"}, {37, "CERT"}, {39, "DNAME"},
			{41, "OPT"}, {42, "APL"}, {43, "DS"}, {44, "SSHFP"}, {45, "IPSECKEY"},
			{46, "RRSIG"}, {47, "NSEC"}, {48, "DNSKEY"}, {49, "DHCID"},
			{50, "NSEC3"}, {51, "NSEC3PARAM"}, {52, "TLSA"}, {53, "SMIMEA"},
			{55, "HIP"}, {59, "CDS"}, {60, "CDNSKEY"}, {61, "OPENPGPKEY"},
			{62, "CSYNC"}, {63, "ZONEMD"}, {64, "SVCB"}, {65, "HTTPS"},
			{99, "SPF"}, {249, "TKEY"}, {250, "TSIG"}, {251, "IXFR"},
			{252, "AXFR"}, {255, "ANY"}, {256, "URI"}, {257, "CAA"},
			{32768, "TA"}, {32769, "DLV"},
		};
		return type_map;
	}

	std::string dns_encode_name(const std::string& name) noexcept
	{
		std::string encoded;
		if (name.empty() || name == ".")
		{
			encoded.push_back('\0');
			return encoded;
		}
		std::string s = name;
		// 去掉末尾的 .
		if (!s.empty() && s.back() == '.')
			s.pop_back();

		size_t pos = 0;
		while (pos < s.size())
		{
			auto dot = s.find('.', pos);
			if (dot == std::string::npos)
				dot = s.size();
			auto label_len = dot - pos;
			if (label_len > 63)
				label_len = 63;
			encoded.push_back(static_cast<char>(static_cast<uint8_t>(label_len)));
			encoded.append(s.data() + pos, label_len);
			pos = dot + 1;
			if (dot == s.size())
				break;
		}
		encoded.push_back('\0');
		return encoded;
	}

	uint16_t dns_type_from_string(const std::string& type_name) noexcept
	{
		const auto& type_map = dns_type_map();
		for (const auto& [num, name] : type_map)
		{
			if (name == type_name)
				return num;
		}
		// 尝试数字.
		try { return static_cast<uint16_t>(std::stoul(type_name)); }
		catch (...) { return DNS_TYPE_A; }
	}

	std::string dns_type_to_string(uint16_t type) noexcept
	{
		const auto& type_map = dns_type_map();
		auto it = type_map.find(type);
		if (it != type_map.end())
			return it->second;
		return std::to_string(type);
	}

	std::string build_dns_wire_query(
		const std::string& name, uint16_t type,
		bool cd, bool do_bit) noexcept
	{
		std::string query;
		query.reserve(512);

		// Header (12 bytes).
		uint16_t id = static_cast<uint16_t>(
			global_random_device()() & 0xFFFF);
		char hdr[12];
		char* hp = hdr;
		write<uint16_t>(id, hp);               // ID
		write<uint16_t>(0x0100, hp);            // Flags: RD=1
		write<uint16_t>(1, hp);                 // QDCOUNT = 1
		write<uint16_t>(0, hp);                 // ANCOUNT = 0
		write<uint16_t>(0, hp);                 // NSCOUNT = 0
		if (do_bit)
			write<uint16_t>(1, hp);             // ARCOUNT = 1 (OPT)
		else
			write<uint16_t>(0, hp);             // ARCOUNT = 0
		query.append(hdr, 12);

		// Question: QNAME.
		auto qname = dns_encode_name(name);
		query.append(qname);

		// Question: QTYPE + QCLASS.
		char qs[4];
		char* qp = qs;
		write<uint16_t>(type, qp);
		write<uint16_t>(DNS_CLASS_IN, qp);
		query.append(qs, 4);

		// OPT pseudo-record for DNSSEC DO bit.
		if (do_bit)
		{
			char opt[11];
			char* op = opt;
			write<uint8_t>(0, op);              // NAME: root (1 byte)
			write<uint16_t>(41, op);             // TYPE: OPT
			write<uint16_t>(1280, op);           // CLASS: UDP payload size
			write<uint16_t>(0x8000, op);         // TTL high: DO=1
			write<uint16_t>(0, op);              // TTL low
			write<uint16_t>(0, op);              // RDLEN = 0
			query.append(opt, 11);
		}

		// 若设置了 CD 标志, 修改 flags.
		if (cd)
		{
			// Flags at offset 2-3.
			uint16_t flags = 0x0100 | 0x0010; // RD=1, CD=1
			char* fp = &query[2];
			write<uint16_t>(flags, fp);
		}

		return query;
	}

	std::pair<std::string, const char*>
	dns_parse_name(const char* p, const char* end, const char* msg_start) noexcept
	{
		std::string name;
		bool jumped = false;
		const char* next = nullptr;

		// 压缩指针跳转次数上限, 防止恶意报文中的指针环导致死循环.
		constexpr int k_max_compression_jumps = 16;
		int jumps = 0;

		while (p < end)
		{
			uint8_t len = static_cast<uint8_t>(*p);
			if (len == 0)
			{
				if (!jumped) next = p + 1;
				break;
			}
			if ((len & 0xC0) == 0xC0)
			{
				// 指针跳转: 限制次数并校验目标偏移, 防止越界或循环.
				if (++jumps > k_max_compression_jumps)
					break;
				if (p + 1 >= end) break;
				uint16_t offset =
					((static_cast<uint16_t>(len & 0x3F)) << 8) |
					static_cast<uint8_t>(*(p + 1));
				if (offset >= static_cast<uint16_t>(end - msg_start))
					break;
				if (!jumped) next = p + 2;
				p = msg_start + offset;
				jumped = true;
				continue;
			}
			if (p + 1 + len > end) break;
			if (!name.empty()) name += '.';
			name.append(p + 1, p + 1 + len);
			p += 1 + len;
		}

		if (!name.empty())
		{
			if (!jumped && next == nullptr && p < end)
				next = p + 1; // 正常结束于 0 长度标签.
			name += '.';
		}
		else if (p < end && *p == 0)
		{
			name = ".";
			if (!jumped) next = p + 1;
		}

		return {name, next ? next : p};
	}

	// dns_svcparams_to_string 解析 HTTPS/SVCB 记录的 SvcParams (RFC 9460).
	// p 指向 SvcParams 起始位置, end 指向 RDATA 结束位置.
	std::string dns_svcparams_to_string(
		const char* p, const char* end) noexcept
	{
		std::string result;
		while (p + 4 <= end)
		{
			auto key = read<uint16_t>(p);
			auto len = read<uint16_t>(p);
			if (p + len > end) break;

			if (!result.empty()) result += " ";

			switch (key)
			{
			case 0: // mandatory
			{
				result += "mandatory=";
				std::string keys;
				const char* kv = p;
				while (kv + 2 <= p + len)
				{
					auto k = read<uint16_t>(kv);
					if (!keys.empty()) keys += ",";
					keys += std::to_string(k);
				}
				result += keys;
				break;
			}
			case 1: // alpn
			{
				result += "alpn=";
				std::string alpn;
				const char* av = p;
				const char* const ae = p + len;
				while (av < ae)
				{
					uint8_t alpn_len = static_cast<uint8_t>(*av++);
					if (av + alpn_len > ae) break;
					if (!alpn.empty()) alpn += ",";
					alpn.append(av, alpn_len);
					av += alpn_len;
				}
				result += alpn;
				break;
			}
			case 2: // no-default-alpn
				result += "no-default-alpn";
				break;
			case 3: // port
				if (len >= 2)
				{
					const char* pv = p;
					result += "port=" + std::to_string(read<uint16_t>(pv));
				}
				break;
			case 4: // ipv4hint
			{
				result += "ipv4hint=";
				std::string ips;
				const char* iv = p;
				while (iv + 4 <= p + len)
				{
					if (!ips.empty()) ips += ",";
					ips += net::ip::make_address_v4(
						read<uint32_t>(iv)).to_string();
				}
				result += ips;
				break;
			}
			case 5: // ech
			{
				result += "ech=";
				std::string hex;
				for (uint16_t i = 0; i < len; i++)
				{
					char buf[3];
					std::snprintf(buf, sizeof(buf), "%02x",
						static_cast<uint8_t>(p[i]));
					hex += buf;
				}
				result += hex;
				break;
			}
			case 6: // ipv6hint
			{
				result += "ipv6hint=";
				std::string ips;
				const char* iv = p;
				while (iv + 16 <= p + len)
				{
					net::ip::address_v6::bytes_type bytes;
					for (auto& b : bytes)
						b = read<uint8_t>(iv);
					if (!ips.empty()) ips += ",";
					ips += net::ip::make_address_v6(bytes).to_string();
				}
				result += ips;
				break;
			}
			default:
			{
				result += std::to_string(key) + "=";
				std::string hex;
				for (uint16_t i = 0; i < len; i++)
				{
					char buf[3];
					std::snprintf(buf, sizeof(buf), "%02x",
						static_cast<uint8_t>(p[i]));
					hex += buf;
				}
				result += hex;
				break;
			}
			}

			p += len;
		}
		return result;
	}

	std::string dns_rdata_to_string(
		uint16_t type, uint16_t rdlength,
		const char* rdata, const char* end,
		const char* msg_start) noexcept
	{
		if (rdata + rdlength > end)
			return {rdata, (size_t)(end - rdata)};

		switch (type)
		{
		case DNS_TYPE_A:
		{
			if (rdlength < 4) return {};
			auto ip = net::ip::make_address_v4(
				read<uint32_t>(rdata));
			return ip.to_string();
		}
		case DNS_TYPE_AAAA:
		{
			if (rdlength < 16) return {};
			net::ip::address_v6::bytes_type bytes;
			for (auto& b : bytes)
				b = read<uint8_t>(rdata);
			return net::ip::make_address_v6(bytes).to_string();
		}
		case DNS_TYPE_CNAME:
		case DNS_TYPE_NS:
		case DNS_TYPE_PTR:
		{
			auto [name, _] = dns_parse_name(rdata, rdata + rdlength, msg_start);
			(void)_;
			return name;
		}
		case DNS_TYPE_MX:
		{
			if (rdlength < 2) return {};
			auto pref = read<uint16_t>(rdata);
			auto [name, _] = dns_parse_name(rdata, rdata + rdlength, msg_start);
			(void)_;
			return std::to_string(pref) + " " + name;
		}
		case DNS_TYPE_TXT:
		{
			std::string result;
			const char* txt_end = rdata + rdlength;
			while (rdata < txt_end)
			{
				uint8_t len = read<uint8_t>(rdata);
				if (rdata + len > txt_end) break;
				if (!result.empty()) result += "\n";
				result.append(rdata, len);
				rdata += len;
			}
			return result;
		}
		case DNS_TYPE_SOA:
		{
			auto [mname, p1] = dns_parse_name(rdata, rdata + rdlength, msg_start);
			if (!p1) return {};
			rdata = p1;
			auto [rname, p2] = dns_parse_name(rdata, rdata + rdlength, msg_start);
			if (!p2) return mname + " " + rname;
			rdata = p2;
			if (rdata + 20 > rdata + rdlength) return {};
			auto serial = read<uint32_t>(rdata);
			auto refresh = read<uint32_t>(rdata);
			auto retry = read<uint32_t>(rdata);
			auto expire = read<uint32_t>(rdata);
			auto minimum = read<uint32_t>(rdata);
			return mname + " " + rname + " " +
				std::to_string(serial) + " " +
				std::to_string(refresh) + " " +
				std::to_string(retry) + " " +
				std::to_string(expire) + " " +
				std::to_string(minimum);
		}
		case DNS_TYPE_SRV:
		{
			if (rdlength < 6) return {};
			auto priority = read<uint16_t>(rdata);
			auto weight = read<uint16_t>(rdata);
			auto srv_port = read<uint16_t>(rdata);
			auto [target, _] = dns_parse_name(rdata, rdata + rdlength, msg_start);
			(void)_;
			return std::to_string(priority) + " " +
				std::to_string(weight) + " " +
				std::to_string(srv_port) + " " + target;
		}
		case DNS_TYPE_HTTPS:
		case DNS_TYPE_SVCB:
		{
			if (rdlength < 2) return {};
			const char* const rd_start = rdata;
			auto svc_priority = read<uint16_t>(rdata);
			auto [svc_target, next_pos] = dns_parse_name(rdata, rdata + rdlength, msg_start);

			std::string result = std::to_string(svc_priority) + " " + svc_target;

			// SvcParams (RFC 9460) 紧随 target 之后.
			if (next_pos && next_pos < rd_start + rdlength)
			{
				auto svcparams = dns_svcparams_to_string(
					next_pos, rd_start + rdlength);
				if (!svcparams.empty())
					result += " " + svcparams;
			}

			return result;
		}
		case DNS_TYPE_CAA:
		{
			if (rdlength < 2) return {};
			auto flags = read<uint8_t>(rdata);
			uint8_t tag_len = read<uint8_t>(rdata);
			if (rdata + tag_len > rdata + rdlength) return {};
			std::string tag(rdata, tag_len);
			rdata += tag_len;
			std::string value(rdata, (rdata + rdlength) - rdata);
			return std::to_string(flags) + " " + tag + " \"" + value + "\"";
		}
		default:
			{
				std::string hex;
				for (uint16_t i = 0; i < rdlength; i++)
				{
					char buf[3];
					std::snprintf(buf, sizeof(buf), "%02x",
						static_cast<uint8_t>(rdata[i]));
					hex += buf;
				}
				return hex;
			}
		}
	}

	std::string dns_response_to_json(
		const std::string& wire_response,
		const std::string& question_name,
		uint16_t question_type) noexcept
	{
		boost::json::object root;

		const char* p = wire_response.data();
		const char* end = p + wire_response.size();
		const char* msg_start = p;

		if (wire_response.size() < 12)
		{
			root["Status"] = -1;
			root["Comment"] = "Response too short";
			return boost::json::serialize(root);
		}

		// Parse header.
		[[maybe_unused]] auto id = read<uint16_t>(p);
		auto flags = read<uint16_t>(p);
		auto qdcount = read<uint16_t>(p);
		auto ancount = read<uint16_t>(p);
		auto nscount = read<uint16_t>(p);
		auto arcount = read<uint16_t>(p);

		auto rcode = flags & 0x0F;
		bool tc = (flags & 0x0200) != 0;
		bool rd = (flags & 0x0100) != 0;
		bool ra = (flags & 0x0080) != 0;
		bool ad = (flags & 0x0020) != 0;
		bool cd = (flags & 0x0010) != 0;

		root["Status"] = rcode;
		root["TC"] = tc;
		root["RD"] = rd;
		root["RA"] = ra;
		root["AD"] = ad;
		root["CD"] = cd;

		// Questions.
		boost::json::array questions;
		for (uint16_t i = 0; i < qdcount && p < end; i++)
		{
			auto [qname, np] = dns_parse_name(p, end, msg_start);
			if (!np) break;
			p = np;
			if (p + 4 > end) break;
			auto qtype = read<uint16_t>(p);
			[[maybe_unused]] auto qclass = read<uint16_t>(p);

			boost::json::object q;
			q["name"] = qname;
			q["type"] = qtype;
			questions.push_back(std::move(q));
		}
		root["Question"] = std::move(questions);

		// Answers.
		boost::json::array answers;
		for (uint16_t i = 0; i < ancount && p < end; i++)
		{
			auto [aname, np] = dns_parse_name(p, end, msg_start);
			if (!np) break;
			p = np;
			if (p + 10 > end) break;
			auto atype = read<uint16_t>(p);
			[[maybe_unused]] auto aclass = read<uint16_t>(p);
			auto attl = read<uint32_t>(p);
			auto rdlength = read<uint16_t>(p);
			if (p + rdlength > end) break;

			boost::json::object a;
			a["name"] = aname;
			a["type"] = atype;
			a["TTL"] = attl;
			a["data"] = dns_rdata_to_string(atype, rdlength, p, end, msg_start);
			answers.push_back(std::move(a));

			p += rdlength;
		}
		root["Answer"] = std::move(answers);

		// Authority.
		boost::json::array authorities;
		for (uint16_t i = 0; i < nscount && p < end; i++)
		{
			auto [nsname, np] = dns_parse_name(p, end, msg_start);
			if (!np) break;
			p = np;
			if (p + 10 > end) break;
			auto nstype = read<uint16_t>(p);
			[[maybe_unused]] auto nsclass = read<uint16_t>(p);
			auto nsttl = read<uint32_t>(p);
			auto nsrdlength = read<uint16_t>(p);
			if (p + nsrdlength > end) break;

			boost::json::object ns;
			ns["name"] = nsname;
			ns["type"] = nstype;
			ns["TTL"] = nsttl;
			ns["data"] = dns_rdata_to_string(nstype, nsrdlength, p, end, msg_start);
			authorities.push_back(std::move(ns));

			p += nsrdlength;
		}
		root["Authority"] = std::move(authorities);

		// Additional.
		boost::json::array additional;
		for (uint16_t i = 0; i < arcount && p < end; i++)
		{
			auto [adname, np] = dns_parse_name(p, end, msg_start);
			if (!np) break;
			p = np;
			if (p + 10 > end) break;
			auto adtype = read<uint16_t>(p);
			[[maybe_unused]] auto adclass = read<uint16_t>(p);
			auto adttl = read<uint32_t>(p);
			auto adrdlength = read<uint16_t>(p);
			if (p + adrdlength > end) break;

			if (adtype != 41)
			{
				boost::json::object ad;
				ad["name"] = adname;
				ad["type"] = adtype;
				ad["TTL"] = adttl;
				ad["data"] = dns_rdata_to_string(adtype, adrdlength, p, end, msg_start);
				additional.push_back(std::move(ad));
			}

			p += adrdlength;
		}
		if (!additional.empty())
			root["Additional"] = std::move(additional);

		root["Comment"] = "Response from proxy DNS";

		return boost::json::serialize(root);
	}

	// dns_answer_summary 解析响应中的 Answer 记录，返回简洁摘要用于日志
	// 输出（如 "A 93.184.216.34 (ttl=60), AAAA 2606:2800:220:1::248:1893
	// (ttl=60)"）。无应答或无法解析时返回空串.
	std::string dns_answer_summary(const std::string& resp) noexcept
	{
		if (resp.size() < 12)
			return {};

		const char* p = resp.data();
		const char* end = p + resp.size();
		const char* msg_start = p;

		p += 2;  // 跳过事务 ID.
		[[maybe_unused]] auto flags = read<uint16_t>(p);
		auto qdcount = read<uint16_t>(p);
		auto ancount = read<uint16_t>(p);
		[[maybe_unused]] auto nscount = read<uint16_t>(p);
		[[maybe_unused]] auto arcount = read<uint16_t>(p);

		// 跳过问题区（QDCOUNT 条 question，每条 = NAME + QTYPE + QCLASS）.
		for (uint16_t i = 0; i < qdcount; i++)
		{
			auto [_, np] = dns_parse_name(p, end, msg_start);
			if (!np || np + 4 > end)
				return {};
			p = np + 4;
		}

		std::string result;
		for (uint16_t i = 0; i < ancount; i++)
		{
			auto [_, np] = dns_parse_name(p, end, msg_start);
			if (!np)
				break;
			p = np;
			if (p + 10 > end)
				break;
			auto atype = read<uint16_t>(p);
			[[maybe_unused]] auto aclass = read<uint16_t>(p);
			auto attl = read<uint32_t>(p);
			auto rdlength = read<uint16_t>(p);
			if (p + rdlength > end)
				break;

			auto data = dns_rdata_to_string(atype, rdlength, p, end, msg_start);
			// TXT 等多段数据以换行分隔，日志中替换为空格更易读.
			for (auto& c : data)
			{
				if (c == '\n')
					c = ' ';
			}

			if (!result.empty())
				result += ", ";
			result += dns_type_to_string(atype) + " " + data +
				" (ttl=" + std::to_string(attl) + ")";

			p += rdlength;
		}
		return result;
	}

	// dns_parse_query 从 wire-format 查询报文解析查询域名与类型.
	// 成功返回 true，name/type 为解析结果；失败（报文过短或无法解析）返回 false.
	bool dns_parse_query(
		const std::string& query, std::string& name, uint16_t& type) noexcept
	{
		if (query.size() < 12)
			return false;

		const char* p = query.data() + 12;
		const char* end = query.data() + query.size();
		const char* msg_start = query.data();

		auto [qname, np] = dns_parse_name(p, end, msg_start);
		if (!np || np + 4 > end)
			return false;

		name = qname;
		// 去掉末尾的 '.'.
		if (!name.empty() && name.back() == '.')
			name.pop_back();

		type = io_util::read<uint16_t>(np);
		return true;
	}

	// dns_query_flags 提取查询报文中的 CD 与 DO 标志位.
	// CD：头部 flags 的 bit4（0x0010）。DO：附加区 OPT 记录 TTL 低 16 位
	// （flags）的 bit15，即 OPT 记录 TYPE 之后第 6 个字节的 bit7。
	void dns_query_flags(
		const std::string& query, bool& cd, bool& do_bit) noexcept
	{
		cd = false;
		do_bit = false;

		if (query.size() < 12)
			return;

		const char* p = query.data();
		const char* end = p + query.size();
		const char* msg_start = p;

		p += 2;
		auto flags = io_util::read<uint16_t>(p);
		if (flags & 0x0010)
			cd = true;

		auto qdcount = io_util::read<uint16_t>(p);
		auto ancount = io_util::read<uint16_t>(p);
		auto nscount = io_util::read<uint16_t>(p);
		auto arcount = io_util::read<uint16_t>(p);

		// 跳过问题区（QDCOUNT 条 question，每条 = NAME + QTYPE + QCLASS）.
		for (uint16_t i = 0; i < qdcount; i++)
		{
			auto [_, np] = dns_parse_name(p, end, msg_start);
			if (!np)
				return;
			p = np;
			if (p + 4 > end)
				return;
			p += 4;
		}

		// 跳过 answer/authority 记录.
		auto skip = static_cast<uint16_t>(ancount + nscount);
		for (uint16_t i = 0; i < skip; i++)
		{
			auto [_, np] = dns_parse_name(p, end, msg_start);
			if (!np)
				return;
			p = np;
			if (p + 10 > end)
				return;
			const char* rd_p = p + 8;
			auto rdlength = io_util::read<uint16_t>(rd_p);
			p += 10 + rdlength;
		}

		// 遍历附加区查找 OPT 记录（type 41）.
		for (uint16_t i = 0; i < arcount; i++)
		{
			auto [_, np] = dns_parse_name(p, end, msg_start);
			if (!np)
				return;
			p = np;
			if (p + 10 > end)
				return;
			auto rtype = io_util::read<uint16_t>(p);
			const char* rd_p = p + 8;
			auto rdlength = io_util::read<uint16_t>(rd_p);
			if (rtype == 41 && p + 8 <= end)
			{
				// OPT 记录：NAME(1) TYPE(2) CLASS(2) TTL(4) RDLEN(2)。
				// TTL 低 16 位为 flags，DO 是 flags 的 bit15，即 TTL 第 3
				// 字节（p+6）的 bit7.
				if (static_cast<uint8_t>(p[6]) & 0x80)
					do_bit = true;
			}
			p += 10 + rdlength;
		}
	}

	// dns_cache_key 生成 DNS 缓存键（域名 + 类型 + CD/DO 标志）.
	// CD/DO 标志影响响应内容（DO=1 含 RRSIG，CD=1 跳过验证），
	// 不同标志的查询不能复用彼此的缓存.
	std::string dns_cache_key(
		const std::string& name, uint16_t type, bool cd, bool do_bit) noexcept
	{
		int flags = 0;
		if (cd)
			flags |= 1;
		if (do_bit)
			flags |= 2;
		return name + "/" + std::to_string(type) + "/" + std::to_string(flags);
	}

	// dns_strip_id 返回剥离事务 ID 的响应副本（缓存存储用）.
	std::string dns_strip_id(const std::string& resp) noexcept
	{
		if (resp.size() < 2)
			return resp;
		std::string out = resp;
		out[0] = '\0';
		out[1] = '\0';
		return out;
	}

	// dns_set_id 返回写入指定事务 ID 的响应副本（缓存命中回包用）.
	std::string dns_set_id(const std::string& resp, uint16_t id) noexcept
	{
		if (resp.size() < 2)
			return resp;
		std::string out = resp;
		out[0] = static_cast<char>((id >> 8) & 0xff);
		out[1] = static_cast<char>(id & 0xff);
		return out;
	}

	// dns_cacheable 判断响应是否可缓存：SERVFAIL（rcode=2）是上游临时
	// 故障，不缓存.
	bool dns_cacheable(const std::string& resp) noexcept
	{
		if (resp.size() < 4)
			return false;
		auto flags = static_cast<uint16_t>(
			(static_cast<uint8_t>(resp[2]) << 8) |
			static_cast<uint8_t>(resp[3]));
		return (flags & 0x0F) != 2;
	}

	// dns_build_response 根据查询报文构建 DNS wire-format 响应，回显问题并
	// 携带应答。rcode 为响应码（0=NOERROR, 1=FORMERR, 2=SERVFAIL,
	// 3=NXDOMAIN）。查询报文过短或无法解析时返回空串.
	std::string dns_build_response(
		const std::string& query, int rcode,
		const std::vector<dns_answer>& answers) noexcept
	{
		if (query.size() < 12)
			return {};

		const char* p = query.data();
		const char* end = p + query.size();
		const char* msg_start = p;

		auto qid = io_util::read<uint16_t>(p);
		// 回显 RD 标志.
		bool rd = (static_cast<uint8_t>(query[2]) & 0x01) != 0;
		p = msg_start + 12;

		auto [qname, next] = dns_parse_name(p, end, msg_start);
		if (!next || next + 4 > end)
			return {};
		auto qtype = io_util::read<uint16_t>(next);
		auto qclass = io_util::read<uint16_t>(next);

		std::string resp;
		resp.reserve(512);

		// Header（12 字节）.
		char hdr[12];
		char* hp = hdr;
		io_util::write<uint16_t>(qid, hp);            // ID
		auto flags = static_cast<uint16_t>(0x8000);   // QR=1
		if (rd)
			flags |= 0x0100;                          // 回显 RD
		flags |= 0x0080;                              // RA=1
		flags |= static_cast<uint16_t>(rcode & 0x0F);
		io_util::write<uint16_t>(flags, hp);
		io_util::write<uint16_t>(1, hp);              // QDCOUNT = 1
		io_util::write<uint16_t>(
			static_cast<uint16_t>(answers.size()), hp); // ANCOUNT
		io_util::write<uint16_t>(0, hp);              // NSCOUNT = 0
		io_util::write<uint16_t>(0, hp);              // ARCOUNT = 0
		resp.append(hdr, 12);

		// 回显问题.
		resp += dns_encode_name(qname);
		char qs[4];
		char* qsp = qs;
		io_util::write<uint16_t>(qtype, qsp);
		io_util::write<uint16_t>(qclass, qsp);
		resp.append(qs, 4);

		// 应答记录.
		for (const auto& a : answers)
		{
			resp += dns_encode_name(a.name);
			char r[10];
			char* rp = r;
			io_util::write<uint16_t>(a.type, rp);
			io_util::write<uint16_t>(DNS_CLASS_IN, rp);
			io_util::write<uint32_t>(a.ttl, rp);
			io_util::write<uint16_t>(
				static_cast<uint16_t>(a.data.size()), rp);
			resp.append(r, 10);
			resp += a.data;
		}

		return resp;
	}

	//////////////////////////////////////////////////////////////////////////

	dns_server::dns_server(
		net::any_io_executor executor,
		net::io_context& backend_context,
		bool scheduler_locking,
		proxy_server_option option)
		: m_executor(std::move(executor))
		, m_backend_context(backend_context)
		, m_scheduler_locking(scheduler_locking)
		, m_option(std::move(option))
	{
		rebuild_cache();
		update_bind_interface();
	}

	void dns_server::start() noexcept
	{
		if (m_option.dns_udp_port_ <= 0)
			return;
		start_listen();
	}

	void dns_server::close() noexcept
	{
		m_abort = true;
		stop_listen();
	}

	std::string dns_server::apply_options(const proxy_server_option& opt)
	{
		// 缓存参数变化时才重建缓存：launcher 每次 set_config 都携带完整配置，
		// 若每次都重建会频繁清空 DNS 缓存，降低缓存命中率.
		bool cache_changed =
			opt.dns_cache_size_ != m_option.dns_cache_size_ ||
			opt.dns_cache_ttl_ != m_option.dns_cache_ttl_;

		bool was_listening = m_udp_socket != nullptr;
		int old_port = m_option.dns_udp_port_;
		int new_port = opt.dns_udp_port_;

		m_option = opt;
		if (cache_changed)
			rebuild_cache();
		update_bind_interface();

		// 端口热改：启动 / 停止 / 切换监听.
		if (new_port > 0 && !was_listening)
		{
			start_listen();
		}
		else if (new_port <= 0 && was_listening)
		{
			stop_listen();
		}
		else if (new_port > 0 && was_listening && new_port != old_port)
		{
			stop_listen();
			start_listen();
		}

		return {};
	}

	dns_response_cache* dns_server::cache() noexcept
	{
		return m_cache.get();
	}

	bool dns_server::no_ipv6() const noexcept
	{
		return m_option.dns_no_ipv6_;
	}

	// rebuild_cache 根据当前配置重建缓存（size/ttl 变化时清空重建）.
	void dns_server::rebuild_cache() noexcept
	{
		if (m_option.dns_cache_size_ > 0 && m_option.dns_cache_ttl_ > 0)
			m_cache = std::make_unique<dns_response_cache>(
				m_option.dns_cache_size_, m_option.dns_cache_ttl_);
		else
			m_cache.reset();
	}

	// update_bind_interface 根据 m_option.local_ip_ 解析向外发起请求时的
	// 出口绑定地址；local_ip_ 为空或解析失败时重置（由系统路由自动选择源地址）.
	void dns_server::update_bind_interface() noexcept
	{
		if (m_option.local_ip_.empty())
		{
			m_bind_interface.reset();
			return;
		}

		boost::system::error_code ec;
		auto bind_if = net::ip::make_address(m_option.local_ip_, ec);
		if (ec)
		{
			// bind 地址有问题, 忽略 bind 参数, 并输出日志.
			XLOG_WARN << "dns server bind address: " << m_option.local_ip_
				<< ", invalid: " << ec.message();
			m_bind_interface.reset();
		}
		else
		{
			m_bind_interface = bind_if;
		}
	}

	// start_listen 创建 UDP socket 并绑定 dns_udp_port_ 端口.
	void dns_server::start_listen() noexcept
	{
		if (m_udp_socket)
			return;

		boost::system::error_code ec;

		auto sock = std::make_shared<udp::socket>(m_executor);
		sock->open(udp::v4(), ec);
		if (ec)
		{
			XLOG_ERR << "udp dns open failed: " << ec.message();
			return;
		}

		sock->set_option(udp::socket::reuse_address(true), ec);

		sock->bind(udp::endpoint(udp::v4(), m_option.dns_udp_port_), ec);
		if (ec)
		{
			XLOG_ERR << "udp dns bind port " << m_option.dns_udp_port_
				<< " failed: " << ec.message();
			return;
		}

		m_udp_socket = sock;
		XLOG_INFO << "udp dns listening on " << sock->local_endpoint(ec);

		net::co_spawn(m_executor, udp_listen(sock), net::detached);
	}

	// stop_listen 关闭当前 UDP 监听 socket，使监听协程退出.
	void dns_server::stop_listen() noexcept
	{
		if (auto sock = m_udp_socket)
		{
			// 立即解除对监听 socket 的引用，使随后调用的 start_listen() 能
			// 创建新的监听 socket。旧 socket 由监听协程通过 shared_ptr 持有，
			// 关闭后协程立即退出并释放，不会悬挂.
			m_udp_socket.reset();
			boost::system::error_code ec;
			sock->close(ec);
		}
	}

	// udp_listen UDP DNS 请求接收主循环.
	net::awaitable<void> dns_server::udp_listen(std::shared_ptr<udp::socket> sock)
	{
		boost::system::error_code ec;

		while (!m_abort)
		{
			std::array<char, 4096> recv_buf{};
			udp::endpoint peer;

			auto recv_len = co_await sock->async_receive_from(
				net::buffer(recv_buf), peer, net_awaitable[ec]);
			if (ec)
				break;  // socket 被关闭（close/热改），退出.

			if (recv_len == 0)
				continue;

			// 复制查询报文，避免并发协程共享读缓冲.
			std::string query(recv_buf.data(), recv_len);

			// 有界并发：限制在途查询协程数量，超出直接丢弃.
			if (m_inflight.fetch_add(1) >= k_dns_max_inflight)
			{
				m_inflight.fetch_sub(1);
				XLOG_WARN << "udp dns: in-flight queries full ("
					<< k_dns_max_inflight << "), dropping packet from " << peer;
				continue;
			}

			net::co_spawn(m_executor,
				[this, sock, peer, query = std::move(query)]() -> net::awaitable<void>
				{
					// 结束时递减在途计数.
					struct inflight_guard
					{
						std::atomic<int>& counter;
						~inflight_guard() { counter.fetch_sub(1); }
					} guard{ m_inflight };

					co_await handle_query(sock, peer, std::move(query));
				}, net::detached);
		}

		// 协程退出时清理（仅当仍是当前监听 socket）.
		if (m_udp_socket == sock)
			m_udp_socket.reset();

		co_return;
	}

	// send_response 向 peer 回送 DNS 响应报文，成功返回 true.
	net::awaitable<bool> dns_server::send_response(
		const std::shared_ptr<udp::socket>& sock,
		const udp::endpoint& peer, const std::string& response)
	{
		boost::system::error_code ec;
		co_await sock->async_send_to(
			net::buffer(response), peer, net_awaitable[ec]);
		if (ec)
		{
			XLOG_WARN << "udp dns write response error: " << ec.message();
			co_return false;
		}
		co_return true;
	}

	// query_upstream 经配置的上游转发 DNS 查询，成功返回 true.
	net::awaitable<bool> dns_server::query_upstream(
		const std::string& dns_query, std::string& output)
	{
		auto& upstream = *m_option.dns_upstream_;
		if (boost::istarts_with(upstream, "https://") ||
			boost::istarts_with(upstream, "http://"))
			co_return co_await doh_query_raw(dns_query, output);
		co_return co_await udp_query_raw(dns_query, output);
	}

	// handle_query 处理单个 UDP DNS 请求：配置了 dns_upstream 时转发到上游，
	// 否则按系统默认解析流程构造响应.
	net::awaitable<void> dns_server::handle_query(
		const std::shared_ptr<udp::socket>& sock,
		const udp::endpoint& peer, std::string query)
	{
		boost::system::error_code ec;

		std::string qname;
		uint16_t qtype = DNS_TYPE_A;
		bool cd = false;
		bool do_flag = false;
		dns_parse_query(query, qname, qtype);
		dns_query_flags(query, cd, do_flag);

		// 禁用 IPv6 解析返回：AAAA 查询直接返回空应答（NODATA），不转发上游.
		if (m_option.dns_no_ipv6_ && qtype == DNS_TYPE_AAAA)
		{
			auto resp = dns_build_response(query, 0, {});
			if (!resp.empty())
			{
				if (!co_await send_response(sock, peer, resp))
					co_return;
			}
			XLOG_DBG << "udp dns query: " << qname << " type "
				<< dns_type_to_string(qtype)
				<< " from " << peer << ", ipv6 disabled, return empty";
			co_return;
		}

		// 缓存键（域名 + 类型 + CD/DO 标志）.
		std::string cache_key;
		if (m_cache && !qname.empty())
			cache_key = dns_cache_key(qname, qtype, cd, do_flag);

		// 缓存命中：改写事务 ID 后直接回包.
		if (m_cache && !cache_key.empty())
		{
			if (auto hit = m_cache->get(cache_key); hit)
			{
				uint16_t qid = static_cast<uint16_t>(
					(static_cast<uint8_t>(query[0]) << 8) |
					static_cast<uint8_t>(query[1]));
				auto resp = dns_set_id(*hit, qid);
				if (!co_await send_response(sock, peer, resp))
					co_return;
				XLOG_DBG << "udp dns query: " << qname << " type "
					<< dns_type_to_string(qtype)
					<< " from " << peer << ", cache hit";
				co_return;
			}
		}

		// 转发上游或本地解析.
		std::string response;
		if (m_option.dns_upstream_ && !m_option.dns_upstream_->empty())
		{
			if (!co_await query_upstream(query, response))
				response = dns_build_response(query, 2, {}); // SERVFAIL
		}
		else
		{
			co_await resolve_normal(query, response);
		}

		if (response.empty())
			co_return;

		// 写入缓存（剥离事务 ID；SERVFAIL 是临时故障，不缓存）.
		if (m_cache && !cache_key.empty() &&
			dns_cacheable(response))
			m_cache->put(cache_key, dns_strip_id(response));

		if (!co_await send_response(sock, peer, response))
			co_return;

		std::string answer_summary = dns_answer_summary(response);
		XLOG_DBG << "udp dns query: " << qname << " type "
			<< dns_type_to_string(qtype)
			<< " from " << peer
			<< ", answer: "
			<< (answer_summary.empty() ? "none" : answer_summary)
			<< ", done";

		co_return;
	}

	// udp_query_raw 通过 UDP 上游转发 DNS 查询.
	net::awaitable<bool> dns_server::udp_query_raw(
		const std::string& dns_query, std::string& output)
	{
		boost::system::error_code ec;

		auto& upstream = *m_option.dns_upstream_;
		auto colon_pos = upstream.find(':');
		if (colon_pos == std::string::npos)
			co_return false;

		auto dns_host = upstream.substr(0, colon_pos);
		int dns_port = 0;
		try
		{
			dns_port = std::stoi(upstream.substr(colon_pos + 1));
		}
		catch (const std::exception&)
		{
			co_return false;
		}

		// 解析上游地址：IP 直接构造 endpoint，域名（如 dns.google:53）
		// 经 resolve_host 解析后取首个地址，与 DoH 上游支持域名保持一致.
		tcp::resolver::results_type targets;
		if (is_hostname(dns_host))
		{
			targets = co_await resolve_host(dns_host, dns_port);
			if (targets.empty())
				co_return false;
		}
		else
		{
			auto addr = net::ip::make_address(dns_host, ec);
			if (ec)
				co_return false;
			targets = tcp::resolver::results_type::create(
				tcp::endpoint(addr, static_cast<uint16_t>(dns_port)),
				dns_host, "");
		}

		auto dns_socket = std::make_shared<udp::socket>(
			co_await net::this_coro::executor);
		udp::endpoint dns_endpoint(
			targets.begin()->endpoint().address(),
			targets.begin()->endpoint().port());

		dns_socket->open(dns_endpoint.protocol(), ec);
		if (ec)
			co_return false;

		// 若配置了 local_ip_ 出口地址，则绑定源地址后发出，保证 DNS 查询
		// 与业务流量走同一出站链路.
		if (m_bind_interface)
		{
			udp::endpoint bind_endpoint(*m_bind_interface, 0);
			dns_socket->bind(bind_endpoint, ec);
			if (ec)
			{
				XLOG_WARN << "udp dns bind source address: "
					<< m_bind_interface->to_string()
					<< ", error: " << ec.message();
				co_return false;
			}
		}

		co_await dns_socket->async_send_to(
			net::buffer(dns_query), dns_endpoint, net_awaitable[ec]);
		if (ec)
			co_return false;

		// 5 秒超时：上游无响应时关闭 socket，使接收失败返回，防止协程因
		// 上游不响应而永久挂起（占用在途查询计数）.
		net::steady_timer timer(co_await net::this_coro::executor);
		timer.expires_after(std::chrono::seconds(5));
		std::weak_ptr<udp::socket> weak_sock(dns_socket);
		timer.async_wait([weak_sock](const boost::system::error_code& tec) {
			if (!tec)
			{
				if (auto sock = weak_sock.lock())
				{
					boost::system::error_code close_ec;
					sock->close(close_ec);
				}
			}
		});

		// 65535 为 UDP 上 DNS 报文的最大长度：固定 4096 会把携带大 payload
		// （DNSSEC 常见）的响应截断.
		std::array<char, 65535> recv_buf{};
		udp::endpoint recv_endp;
		auto recv_len = co_await dns_socket->async_receive_from(
			net::buffer(recv_buf), recv_endp, net_awaitable[ec]);
		timer.cancel();

		if (ec)
			co_return false;

		dns_socket->close(ec);
		output.assign(recv_buf.data(), recv_len);
		co_return true;
	}

	// resolve_host 解析主机地址（在 backend 执行上下文执行同步解析）.
	net::awaitable<tcp::resolver::results_type>
	dns_server::resolve_host(const std::string& host, uint16_t port)
	{
		boost::system::error_code ec;

		auto ex = co_await backend_switch_to(
			m_scheduler_locking, m_backend_context, m_executor);

		tcp::resolver resolver{ ex };
		auto targets = co_await resolver.async_resolve(
			host, std::to_string(port), net_awaitable[ec]);

		co_await backend_switch_from(m_scheduler_locking, m_executor);

		if (ec)
		{
			XLOG_WARN << "dns server resolve: " << host
				<< ", error: " << ec.message();
			co_return tcp::resolver::results_type{};
		}

		co_return targets;
	}

	// parse_doh_url 解析 DoH 上游 URL，返回 host/port/路径/TLS 主机名.
	// URL 非法或未配置上游时返回 nullopt.
	std::optional<dns_server::doh_url_info> dns_server::parse_doh_url() const
	{
		if (!m_option.dns_upstream_)
			return std::nullopt;

		// 使用 boost.url 标准解析 DoH 上游 URL，获取 host/port/path，以及
		// query 中指定的 SNI（如 https://1.2.3.4/dns-query?sni=example.com
		// 时 SNI 为 example.com）；query 本身不随 POST 请求转发给上游.
		auto u = boost::urls::parse_uri(*m_option.dns_upstream_);
		if (!u)
			return std::nullopt;

		doh_url_info info;
		info.scheme = std::string(u->scheme());
		info.host = std::string(u->encoded_host());
		info.port = u->port_number();
		if (info.port == 0)
			info.port = boost::urls::default_port(u->scheme_id());

		// 请求路径（不含 query）.
		info.path = "/dns-query";
		auto path = u->path();
		if (!path.empty() && path != "/")
			info.path = std::string(path);

		// query 中指定的 SNI（用于 TLS 握手与证书校验）.
		std::string doh_sni;
		if (auto it = u->params().find("sni");
			it != u->params().end())
			doh_sni = std::string((*it).value);

		// TLS 校验与 SNI 使用的主机名：配置了 sni 参数时优先使用.
		info.tls_host = doh_sni.empty() ? info.host : doh_sni;

		return info;
	}

	// resolve_doh_target 解析 DoH 服务器地址（IP 直接构造 endpoint，
	// 域名走 resolve_host 解析）.
	net::awaitable<tcp::resolver::results_type>
	dns_server::resolve_doh_target(const doh_url_info& info)
	{
		if (!is_hostname(info.host))
		{
			tcp::endpoint endp(
				net::ip::make_address(info.host), info.port);
			co_return tcp::resolver::results_type::create(
				endp, std::string(info.host), "");
		}

		co_return co_await resolve_host(info.host, info.port);
	}

	// connect_doh_target 连接到 DoH 服务器；配置了 local_ip_ 出口地址时
	// 绑定源地址后连接，成功返回 true.
	net::awaitable<bool> dns_server::connect_doh_target(
		tcp::socket& doh_socket,
		const tcp::resolver::results_type& targets)
	{
		boost::system::error_code ec;
		ec = boost::asio::error::host_not_found;
		for (const auto& entry : targets)
		{
			auto endp = entry.endpoint();
			if (m_bind_interface)
			{
				// 配置了 local_ip_ 出口地址：绑定源地址后连接，保证 DoH 查询
				// 与业务流量走同一出站链路.
				doh_socket.close(ec);
				tcp::endpoint bind_endpoint(*m_bind_interface, 0);
				doh_socket.open(bind_endpoint.protocol(), ec);
				if (ec)
					continue;
				doh_socket.bind(bind_endpoint, ec);
				if (ec)
				{
					XLOG_WARN << "doh bind source address: "
						<< m_bind_interface->to_string()
						<< ", error: " << ec.message();
					continue;
				}
			}
			co_await doh_socket.async_connect(endp, net_awaitable[ec]);
			if (!ec)
				break;
		}
		if (ec)
			co_return false;

		co_return true;
	}

	// doh_tls_handshake 设置 SNI 并完成与 DoH 服务器的客户端 TLS 握手，
	// 成功返回 true.
	net::awaitable<bool> dns_server::doh_tls_handshake(
		net::ssl::stream<tcp::socket>& ssl_stream,
		const doh_url_info& info)
	{
		boost::system::error_code ec;

		if (!SSL_set_tlsext_host_name(
			ssl_stream.native_handle(), info.tls_host.c_str()))
		{
			XLOG_DBG << "doh set sni name: " << info.tls_host << " failed";
		}

		co_await ssl_stream.async_handshake(
			net::ssl::stream_base::client, net_awaitable[ec]);
		if (ec)
		{
			XLOG_WARN << "doh tls handshake with " << info.host << " failed";
			co_return false;
		}

		co_return true;
	}

	// doh_http_post 经已建立的连接发送 DoH POST 请求并读取响应（Stream
	// 为 TLS 流或明文 socket），成功返回 true.
	template <typename Stream>
	net::awaitable<bool> dns_server::doh_http_post(
		Stream& stream,
		const doh_url_info& info,
		const std::string& dns_query, std::string& output)
	{
		boost::system::error_code ec;

		// 构造 HTTP POST 请求.
		http::request<http::string_body> doh_req{
			http::verb::post, info.path, 11 };
		doh_req.set(http::field::host, info.tls_host);
		doh_req.set(http::field::content_type, "application/dns-message");
		doh_req.set(http::field::accept, "application/dns-message");
		doh_req.body() = dns_query;
		doh_req.prepare_payload();

		co_await http::async_write(stream, doh_req, net_awaitable[ec]);
		if (ec)
			co_return false;

		beast::flat_buffer buf;
		http::response<http::string_body> doh_res;
		co_await http::async_read(stream, buf, doh_res, net_awaitable[ec]);
		if (ec)
			co_return false;

		if (doh_res.result() != http::status::ok)
		{
			XLOG_WARN << "doh query raw response status: "
				<< doh_res.result_int();
			co_return false;
		}

		output = std::move(doh_res.body());
		co_return true;
	}

	// doh_query_raw 通过 DoH (DNS over HTTPS) 上游转发 DNS 查询.
	net::awaitable<bool> dns_server::doh_query_raw(
		const std::string& dns_query, std::string& output)
	{
		boost::system::error_code ec;

		// 解析 DoH 上游 URL.
		auto info = parse_doh_url();
		if (!info)
			co_return false;

		// 解析 DoH 服务器地址.
		auto targets = co_await resolve_doh_target(*info);
		if (targets.empty())
			co_return false;

		// 连接到 DoH 服务器.
		tcp::socket doh_socket(m_executor);
		if (!co_await connect_doh_target(doh_socket, targets))
			co_return false;

		// 明文 http 上游：直接在已连接 socket 上完成 POST 交互，不做 TLS.
		if (info->scheme == "http")
			co_return co_await doh_http_post(
				doh_socket, *info, dns_query, output);

		// 创建 per-request SSL context, 确保每个 DoH 服务器使用正确的主机名校验.
		net::ssl::context doh_ssl_ctx(net::ssl::context::sslv23_client);
		ec = configure_ssl_client_ctx(doh_ssl_ctx,
			m_option.disable_check_cert_,
			info->tls_host);
		if (ec)
		{
			XLOG_WARN << "configure ssl context for doh: " << info->tls_host
				<< " error: " << ec.message();
			co_return false;
		}

		// 建立 TLS 连接并完成 DoH POST 查询交互.
		net::ssl::stream<tcp::socket> ssl_stream(std::move(doh_socket), doh_ssl_ctx);
		if (!co_await doh_tls_handshake(ssl_stream, *info))
			co_return false;

		co_return co_await doh_http_post(ssl_stream, *info, dns_query, output);
	}

	// append_address_answers 从系统解析结果中按 qtype 提取 A/AAAA 记录.
	static void append_address_answers(
		const tcp::resolver::results_type& targets,
		uint16_t qtype, const std::string& qname,
		std::vector<dns_answer>& answers)
	{
		for (const auto& t : targets)
		{
			auto addr = t.endpoint().address();
			if (qtype == DNS_TYPE_A)
			{
				if (!addr.is_v4())
					continue;
				auto bytes = addr.to_v4().to_bytes();
				std::string data(
					reinterpret_cast<const char*>(bytes.data()),
					bytes.size());
				answers.push_back({
					qname + ".", DNS_TYPE_A, dns_local_ttl,
					std::move(data) });
			}
			else
			{
				if (!addr.is_v6())
					continue;
				auto bytes = addr.to_v6().to_bytes();
				std::string data(
					reinterpret_cast<const char*>(bytes.data()),
					bytes.size());
				answers.push_back({
					qname + ".", DNS_TYPE_AAAA, dns_local_ttl,
					std::move(data) });
			}
		}
	}

	// resolve_address_query 通过系统解析 qname 的 A/AAAA 地址并构造应答.
	net::awaitable<void> dns_server::resolve_address_query(
		const std::string& dns_query, const std::string& qname,
		uint16_t qtype, std::string& output)
	{
		boost::system::error_code ec;

		// 在 backend 执行上下文执行同步解析.
		auto ex = co_await backend_switch_to(
			m_scheduler_locking, m_backend_context, m_executor);

		tcp::resolver resolver{ ex };
		auto targets = co_await resolver.async_resolve(
			qname, "", net_awaitable[ec]);

		co_await backend_switch_from(m_scheduler_locking, m_executor);

		// 查询失败统一返回 NXDOMAIN.
		if (ec)
		{
			output = dns_build_response(dns_query, 3, {});
			co_return;
		}

		// 从解析结果中按 qtype 提取匹配的地址记录.
		std::vector<dns_answer> answers;
		append_address_answers(targets, qtype, qname, answers);

		output = dns_build_response(dns_query, 0, answers);
		co_return;
	}

	// resolve_normal 按系统默认解析流程处理 DNS 查询并构造响应.
	net::awaitable<void> dns_server::resolve_normal(
		const std::string& dns_query, std::string& output)
	{
		std::string qname;
		uint16_t qtype = DNS_TYPE_A;
		if (!dns_parse_query(dns_query, qname, qtype))
		{
			// 无法解析的查询返回 FORMERR.
			output = dns_build_response(dns_query, 1, {});
			co_return;
		}

		// 禁用 IPv6 解析返回：AAAA 查询返回空应答（NODATA）.
		if (m_option.dns_no_ipv6_ && qtype == DNS_TYPE_AAAA)
		{
			output = dns_build_response(dns_query, 0, {});
			co_return;
		}

		switch (qtype)
		{
		case DNS_TYPE_A:
		case DNS_TYPE_AAAA:
			co_return co_await resolve_address_query(
				dns_query, qname, qtype, output);
		default:
			// 其余类型（CNAME/MX/TXT/SOA 等）返回 NOERROR 且无应答.
			output = dns_build_response(dns_query, 0, {});
			co_return;
		}
	}

	template net::awaitable<bool> dns_server::doh_http_post<tcp::socket>(
		tcp::socket&, const dns_server::doh_url_info&,
		const std::string&, std::string&);
	template net::awaitable<bool> dns_server::doh_http_post<
		net::ssl::stream<tcp::socket>>(
		net::ssl::stream<tcp::socket>&, const dns_server::doh_url_info&,
		const std::string&, std::string&);

}

//////////////////////////////////////////////////////////////////////////

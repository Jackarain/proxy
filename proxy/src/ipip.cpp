//
// Copyright (C) 2019 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#include <boost/algorithm/string.hpp>

#include "proxy/ipip.hpp"
#include "proxy/logging.hpp"

using byte = unsigned char;
using uint = unsigned int;

#define B2IL(b) (((b)[0] & 0xFF) | (((b)[1] << 8) & 0xFF00) | (((b)[2] << 16) & 0xFF0000) | (((b)[3] << 24) & 0xFF000000))
#define B2IU(b) (((b)[3] & 0xFF) | (((b)[2] << 8) & 0xFF00) | (((b)[1] << 16) & 0xFF0000) | (((b)[0] << 24) & 0xFF000000))

namespace proxy {

	bool ipip_datx::load(const std::string& filename)
	{
		XLOG_INFO << "ipip load " << filename;

		std::ifstream file(filename.c_str(), std::ios::binary);
		if (!file.is_open())
		{
			XLOG_ERR << "load ipip data '"
				<< filename
				<< "' failed";

			return false;
		}

		m_data.assign(std::istreambuf_iterator<char>(file),
			std::istreambuf_iterator<char>());

		uint length = B2IU(m_data.data());

		m_index.resize(length * sizeof(byte));
		memcpy(m_index.data(), m_data.data() + 4, length);

		m_offset = length;

		m_flag.resize(65536 * sizeof(uint));
		memcpy(m_flag.data(), m_index.data(), 65536 * sizeof(uint));

		XLOG_INFO << "load ipip data '"
			<< filename
			<< "' success";

		return true;
	}

	std::tuple<std::vector<std::string>, std::string>
	ipip_datx::lookup(boost::asio::ip::address ip)
	{
		std::vector<std::string> ret;

		if (m_data.empty() || ip.is_v6())
			return {};

		std::string result;

		auto ips = ip.to_v4().to_bytes();

		uint ip_prefix_value = ips[0] * 256 + ips[1];
		uint ip2long_value = B2IU(ips);

		uint start = m_flag[ip_prefix_value];

		uint max_comp_len = m_offset - 262144 - 4;
		uint index_m_offset = 0;
		uint index_length = 0;

		for (start = start * 9 + 262144; start < max_comp_len; start += 9)
		{
			if (B2IU(m_index.data() + start) >= ip2long_value)
			{
				index_m_offset = B2IL(m_index.data() + start + 4) & 0x00FFFFFF;
				index_length = (m_index[start + 7] << 8) + m_index[start + 8];

				break;
			}
		}

		result.resize(index_length);

		memcpy(&result[0],
			&m_data[m_offset + index_m_offset - 262144],
			index_length);

		// segment result into peaces.
		boost::split(ret, result, boost::is_space());

		std::string isp;

		if (ret.size() == 5)
		{
			isp = *ret.rbegin();
			ret.pop_back();
		}

		if (ret.back().empty())
			ret.pop_back();

		return std::make_tuple(ret, isp);
	}
}

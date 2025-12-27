//
// Copyright (C) 2019 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#include "proxy/ip_database.hpp"
#include "proxy/logging.hpp"

#include <boost/algorithm/string.hpp>

#include <boost/json.hpp>
#include <boost/asio/ip/address.hpp>

using byte = unsigned char;
using uint = unsigned int;

constexpr uint32_t be32_to_host(uint32_t x) {
    return ((x & 0x000000FFu) << 24) |
           ((x & 0x0000FF00u) << 8)  |
           ((x & 0x00FF0000u) >> 8)  |
           ((x & 0xFF000000u) >> 24);
}

constexpr int32_t B2IL(const uint8_t* b) {
    return (static_cast<int32_t>(b[0] & 0xFF)) |
           (static_cast<int32_t>(b[1] & 0xFF) << 8) |
           (static_cast<int32_t>(b[2] & 0xFF) << 16) |
           (static_cast<int32_t>(b[3] & 0xFF) << 24);
}

constexpr uint32_t B2IU(const uint8_t* b) {
    return (static_cast<uint32_t>(b[3] & 0xFF)) |
           (static_cast<uint32_t>(b[2] & 0xFF) << 8) |
           (static_cast<uint32_t>(b[1] & 0xFF) << 16) |
           (static_cast<uint32_t>(b[0] & 0xFF) << 24);
}

namespace proxy {

	//////////////////////////////////////////////////////////////////////////

	bool ip_datx::load(const std::string& filename)
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

		uint length = B2IU((const uint8_t*)m_data.data());

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
	ip_datx::lookup(boost::asio::ip::address ip)
	{
		std::vector<std::string> ret;

		if (m_data.empty() || ip.is_v6())
			return {};

		std::string result;

		auto ips = ip.to_v4().to_bytes();

		uint ip_prefix_value = ips[0] * 256 + ips[1];
		uint ip2long_value = B2IU((const uint8_t*)&ips[0]);

		uint start = m_flag[ip_prefix_value];

		uint max_comp_len = m_offset - 262144 - 4;
		uint index_m_offset = 0;
		uint index_length = 0;

		for (start = start * 9 + 262144; start < max_comp_len; start += 9)
		{
			if (B2IU((const uint8_t*)m_index.data() + start) >= ip2long_value)
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


	//////////////////////////////////////////////////////////////////////////

	struct Error : std::runtime_error { using std::runtime_error::runtime_error; };
	static const Error ErrFileOpen("ipdb: file open error");
	static const Error ErrFileSize("ipdb: file size error");
	static const Error ErrDatabase("ipdb: database corrupted");
	static const Error ErrDataEmpty("ipdb: data not exists");

    static std::vector<std::string> split_tab(const std::string& s) {
        std::vector<std::string> res;
        size_t last = 0, next;
        while ((next = s.find('\t', last)) != std::string::npos) {
            res.push_back(s.substr(last, next - last));
            last = next + 1;
        }
        res.push_back(s.substr(last));
        return res;
    }

	bool ip_ipdb::load(const std::string& filename)
	{
       try {
            std::ifstream fs(filename, std::ios::binary);
            if (!fs) throw ErrFileOpen;

            // 1. 读取 4 字节元数据长度
            uint32_t metaLenBE = 0;
            fs.read(reinterpret_cast<char*>(&metaLenBE), 4);
            uint32_t metaLen = be32_to_host(metaLenBE);

            // 2. 读取并解析 JSON 元数据
            std::string metaJson(metaLen, '\0');
            fs.read(&metaJson[0], metaLen);
            if (!fs) throw ErrFileSize;
            parse_meta(metaJson);

            // 3. 校验并读取数据库主体
            fs.seekg(0, std::ios::end);
            if ((int64_t)fs.tellg() != (int64_t)4 + metaLen + totalSize_) throw ErrFileSize;

            data_.resize((size_t)totalSize_);
            fs.seekg(4 + metaLen, std::ios::beg);
            fs.read(reinterpret_cast<char*>(data_.data()), totalSize_);

            // 4. 预计算 IPv4 偏移 (Trie 树 96 位深度处)
            v4offset_ = 0;
            for (int i = 0; i < 96 && v4offset_ < nodeCount_; ++i) {
                v4offset_ = read_node(v4offset_, (i >= 80) ? 1 : 0);
            }

            // 5. 自动配置默认语言和 ISP 字段索引
            currentLang_ = "CN";
            if (languages_.find(currentLang_) == languages_.end() && !languages_.empty()) {
                currentLang_ = languages_.begin()->first;
            }
            ispIdx_ = guess_isp_index();

            loaded_ = true;

        } catch (...) {
            loaded_ = false;
            return false;
        }

		return true;
	}

	std::tuple<std::vector<std::string>, std::string>
	ip_ipdb::lookup(net::ip::address ip)
	{
        if (!loaded_) throw Error("ipdb: not loaded");

        // 1. 根据 IP 类型执行树搜索
        int node = 0;
        if (ip.is_v4()) {
            if (!(ipVersion_ & 0x01)) throw Error("ipdb: no ipv4 support");
            auto bytes = ip.to_v4().to_bytes();
            node = search_tree(bytes.data(), 32, v4offset_);
        } else {
            if (!(ipVersion_ & 0x02)) throw Error("ipdb: no ipv6 support");
            auto bytes = ip.to_v6().to_bytes();
            node = search_tree(bytes.data(), 128, 0);
        }

        // 2. 解析数据区内容
        std::string rawStr = resolve_content(node);
        std::vector<std::string> parts = split_tab(rawStr);

        // 3. 根据语言提取字段
        int langBase = languages_[currentLang_];
        std::vector<std::string> fields;
        for (size_t i = 0; i < fieldNames_.size(); ++i) {
            size_t pIdx = (size_t)langBase + i;
            fields.push_back(pIdx < parts.size() ? parts[pIdx] : "");
        }

        // 4. 整理返回格式
        std::vector<std::string> region;

        for (int i = 0; i < (int)fields.size(); ++i) {
            if (i != ispIdx_ && !fields[i].empty()) region.push_back(fields[i]);
        }

        return {region, ""};
	}

  void ip_ipdb::parse_meta(const std::string& json)
  {
        auto v = boost::json::parse(json);
        auto const& obj = v.as_object();
        nodeCount_ = (int)obj.at("node_count").as_int64();
        totalSize_ = (int)obj.at("total_size").as_int64();
        ipVersion_ = (uint16_t)obj.at("ip_version").as_int64();

        fieldNames_.clear();
        for (auto const& f : obj.at("fields").as_array())
            fieldNames_.emplace_back(f.as_string().c_str());

        languages_.clear();
        for (auto const& kv : obj.at("languages").as_object())
            languages_[std::string(kv.key())] = (int)kv.value().as_int64();
    }

    int ip_ipdb::read_node(int node, int bit) const
	{
        size_t offset = (size_t)node * 8 + (size_t)bit * 4;
        if (offset + 4 > data_.size()) throw ErrDatabase;
        uint32_t val;
        std::memcpy(&val, &data_[offset], 4);
        return (int)be32_to_host(val);
    }

    int ip_ipdb::search_tree(const uint8_t* ip, int bits, int startNode) const
	{
        int node = startNode;
        for (int i = 0; i < bits; ++i) {
            if (node >= nodeCount_) break;
            int bit = (ip[i >> 3] >> (7 - (i & 7))) & 1;
            node = read_node(node, bit);
        }
        if (node >= nodeCount_) return node;
        throw ErrDataEmpty;
    }

    std::string ip_ipdb::resolve_content(int node) const
	{
        // 数据区偏移 = 节点位置 - 节点总数 + (节点总数 * 8字节)
        size_t pos = (size_t)node - nodeCount_ + (size_t)nodeCount_ * 8;
        if (pos + 2 > data_.size()) throw ErrDatabase;

        size_t len = ((size_t)data_[pos] << 8) | (size_t)data_[pos + 1];
        if (pos + 2 + len > data_.size()) throw ErrDatabase;

        return std::string((const char*)data_.data() + pos + 2, len);
    }

    int ip_ipdb::guess_isp_index()
	{
        for (size_t i = 0; i < fieldNames_.size(); ++i) {
            std::string n = fieldNames_[i];
            std::transform(n.begin(), n.end(), n.begin(), ::tolower);
            if (n.find("isp") != std::string::npos || n.find("operator") != std::string::npos)
                return (int)i;
        }
        return -1;
    }

	//////////////////////////////////////////////////////////////////////////

}

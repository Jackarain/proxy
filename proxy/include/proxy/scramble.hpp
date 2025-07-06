//
// scramble.hpp
// ~~~~~~~~~~~~
//
// Copyright (c) 2023 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2023_11_24__SCRAMBLE_HPP
#define INCLUDE__2023_11_24__SCRAMBLE_HPP


#include "proxy/xxhash.hpp"

#include <boost/assert.hpp>

#include <random>
#include <cstdint>
#include <sstream>
#include <vector>
#include <set>
#include <span>
#include <limits>


namespace proxy {

	// 用于制造噪声数据, 具体而言, 就是通过生成随机长度的随机数据, 并将长度信息通过
	// 编码到随机数据前几个字节的最后一位, 以提高数据的隐匿性.
	// 分析者即使通过阅读此源码得知数据的编码方法, 也很难将正常数据与通过此方式得到
	// 的数据快速甄别.
	inline std::vector<uint8_t>
	generate_noise(uint16_t max_len = 0x7FFF, std::set<uint8_t> bfilter = {})
	{
		if (max_len < 16)
			return {};

		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> dis(0, 255);
		std::uniform_int_distribution<> prefix(16, max_len);

		std::vector<uint8_t> data;

		uint16_t length = static_cast<uint16_t>(prefix(gen));

		// 长度信息为 16 位, 因此只需要将长度信息编码到前 16 个字节的最后一位.
		for (int i = 0; i < 16; i++)
		{
			uint8_t c = (static_cast<uint8_t>(dis(gen)) & 0xFE)| ((length >> i) & 1);

			if (i == 0 && !bfilter.empty())
			{
				// 如果生成的第1个随机字节在过滤列表中, 则重新生成.
				while (bfilter.contains(c))
					c = (static_cast<uint8_t>(dis(gen)) & 0xFE)| ((length >> i) & 1);
			}

			data.push_back(c);
		}

		// 继续制造噪声.
		while (data.size() < length)
			data.push_back(static_cast<uint8_t>(dis(gen)));

		return data;
	}

	// 从噪声数据中恢复噪声数据的长度信息.
	inline int extract_noise_length(const std::vector<uint8_t>& data)
	{
		if (data.size() < 16)
			return -1;

		uint16_t length = 0;

		for (int i = 0; i < 16; ++i)
			length |= ((data[i] & 1) << i);

		return length;
	}

	// 用于通过计算噪声数据的 xx128hash 得到一个随机的 key.
	inline std::vector<uint8_t> compute_key(std::span<uint8_t> data)
	{
		XXH3_state_t state;

		XXH3_128bits_reset(&state);
		XXH3_128bits_update(&state, data.data(), data.size());
		auto result = XXH3_128bits_digest(&state);

		std::vector<uint8_t> key(16, 0);
		for (int i = 0; i < 8; i++)
		{
			key[i] = static_cast<uint8_t>(result.high64 >> (i * CHAR_BIT));
			key[i + 8] = static_cast<uint8_t>(result.low64 >> (i * CHAR_BIT));
		}

		return key;
	}

	// 用于对数据进行混淆, 通过 key 与数据进行异或运算, 以达到混淆数据的目的.
	class scramble_stream
	{
		scramble_stream(const scramble_stream&) = delete;
		scramble_stream& operator=(const scramble_stream&) = delete;

	public:
		scramble_stream() = default;
		~scramble_stream() = default;

		scramble_stream(scramble_stream&& other) noexcept
			: m_key(std::move(other.m_key))
			, m_pos(other.m_pos)
		{
			other.m_pos = 0;
			other.m_key.clear();
		}

		scramble_stream& operator=(scramble_stream&& other) noexcept
		{
			m_key = std::move(other.m_key);
			m_pos = other.m_pos;

			other.m_pos = 0;
			other.m_key.clear();

			return *this;
		}

	public:
		inline bool is_valid() const noexcept
		{
			return !m_key.empty();
		}

		inline void reset() noexcept
		{
			m_key.clear();
			m_pos = 0;
		}

		inline void reset(std::span<uint8_t> data) noexcept
		{
			m_key = compute_key(data);
			m_pos = 0;
		}

		inline void set_key(const std::vector<uint8_t>& key) noexcept
		{
			m_key = key;
		}

		// 将数据 data 加解密, 但不改变 scramble_stream 类的状态.
		inline void peek_data(std::span<uint8_t> data) const noexcept
		{
			BOOST_ASSERT(m_key.size() == 16 && "key must be set!");

			if (data.empty() || m_key.empty())
				return;

			std::vector<uint8_t> tmp = m_key;
			size_t i = 0;
			size_t pos = m_pos;

			do {
				data[i] = data[i] ^ tmp[pos];

				pos = ++pos % tmp.size();

				if (pos == 0)
					tmp = compute_key(tmp);

			} while (++i < data.size());
		}

		inline std::vector<uint8_t> scramble(std::span<uint8_t> data) noexcept
		{
			BOOST_ASSERT(m_key.size() == 16 && "key must be set!");

			if (data.empty() || m_key.empty())
				return {};

			std::vector<uint8_t> result(data.size(), 0);
			size_t i = 0;

			do {
				result[i] = data[i] ^ m_key[m_pos];

				m_pos = ++m_pos % m_key.size();

				if (m_pos == 0)
					m_key = compute_key(m_key);

			} while (++i < data.size());

			return result;
		}

		inline virtual void scramble(uint8_t* data, size_t size) noexcept
		{
			BOOST_ASSERT(m_key.size() == 16 && "key must be set!");

			if (!data || size == 0 || m_key.empty())
				return;

			size_t i = 0;

			do {
				data[i] = data[i] ^ m_key[m_pos];

				m_pos = ++m_pos % m_key.size();

				if (m_pos == 0)
					m_key = compute_key(m_key);

			} while (++i < size);
		}

	private:
		std::vector<uint8_t> m_key;
		size_t m_pos = 0;
	};
}

#endif // INCLUDE__2023_11_24__SCRAMBLE_HPP

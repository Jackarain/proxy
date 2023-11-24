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

#include <random>
#include <vector>
#include <set>
#include <span>
#include <limits>

#include <boost/assert.hpp>

#include "proxy/xxhash.hpp"

namespace proxy {

	inline int start_position(std::mt19937& gen)
	{
		const static int pos[] =
		{ 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24 };

		static std::normal_distribution<> dis(5, 4);

		int num = static_cast<int>(dis(gen));
		num = std::clamp(num, 0, 10);

		return pos[num];
	}

	inline std::vector<uint8_t>
	generate_noise(uint16_t max_len = 0x7FFF, std::set<uint8_t> bfilter = {})
	{
		if (max_len <= 4)
			return {};

		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> dis(0, 255);

		std::vector<uint8_t> data;

		uint8_t bhalf = static_cast<uint8_t>(max_len >> CHAR_BIT);
		uint8_t ehalf = static_cast<uint8_t>(max_len & 0xFF);

		int start_pos = start_position(gen);
		if (start_pos > max_len)
			start_pos = (max_len / 2) * 2;

		bool flip = false;
		for (int i = 0; i < start_pos; i++)
		{
			uint8_t c = static_cast<uint8_t>(dis(gen));

			if (flip)
				c |= bhalf;
			else
				c |= ehalf;

			if (i == 0 && !bfilter.empty())
			{
				while (bfilter.contains(c))
				{
					c = static_cast<uint8_t>(dis(gen));
					if (flip)
						c |= bhalf;
					else
						c |= ehalf;
				}
			}

			flip = !flip;

			data.push_back(c);
		}

		uint16_t min_len = std::min<uint16_t>(max_len,
			static_cast<uint16_t>(start_pos));

		if (min_len >= max_len)
			min_len = max_len - 1;

		auto length = std::uniform_int_distribution<>(min_len, max_len - 1)(gen);

		data[start_pos - 2] = static_cast<uint8_t>(length >> CHAR_BIT);
		data[start_pos - 1] = static_cast<uint8_t>(length & 0xFF);

		data[start_pos - 4] |= static_cast<uint8_t>(min_len >> CHAR_BIT);
		data[start_pos - 3] |= static_cast<uint8_t>(min_len & 0xFF);

		uint16_t a = data[start_pos - 3] | (data[start_pos - 4] << CHAR_BIT);
		uint16_t b = data[start_pos - 1] | (data[start_pos - 2] << CHAR_BIT);

		length = a & b;

		while (data.size() < length)
			data.push_back(static_cast<uint8_t>(dis(gen)));

		return data;
	}

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

	inline std::vector<uint8_t> scramble_data(
		const std::vector<uint8_t>& key, std::span<uint8_t> data)
	{
		BOOST_ASSERT(key.size() == 16 && "key must be set!");

		if (data.empty())
			return {};

		std::vector<uint8_t> result(data.size(), 0);
		std::vector<uint8_t> tmp = key;
		size_t i = 0;

		do {
			result[i] = data[i] ^ tmp[i % tmp.size()];

			if (++i % tmp.size() == 0)
				tmp = compute_key(tmp);

		} while (i < data.size());

		return result;
	}

	class scramble_stream
	{
		scramble_stream(const scramble_stream&) = delete;
		scramble_stream& operator=(const scramble_stream&) = delete;

	public:
		explicit scramble_stream(std::span<uint8_t> data)
			: m_key(compute_key(data))
		{}

		~scramble_stream() = default;

	public:
		inline void reset()
		{
			m_key.clear();
			m_pos = 0;
		}

		inline void reset(std::span<uint8_t> data)
		{
			m_key = compute_key(data);
			m_pos = 0;
		}

		inline std::vector<uint8_t> scramble(std::span<uint8_t> data)
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

			} while (i < data.size());

			return result;
		}

	private:
		std::vector<uint8_t> m_key;
		size_t m_pos = 0;
	};
}

#endif // INCLUDE__2023_11_24__SCRAMBLE_HPP

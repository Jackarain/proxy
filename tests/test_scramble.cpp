//
// test_scramble.cpp
// ~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/scramble.hpp"

#include <boost/test/unit_test.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>


namespace {

	using namespace proxy;

	BOOST_AUTO_TEST_SUITE(scramble_suite)

	BOOST_AUTO_TEST_CASE(generate_noise_round_trips_length)
	{
		std::random_device rd;

		for (uint16_t max_len : { 16, 64, 1000 })
		{
			auto noise = generate_noise(rd, max_len);

			BOOST_TEST(!noise.empty());
			BOOST_TEST(noise.size() >= 16u);
			BOOST_TEST(noise.size() <= max_len);
			BOOST_TEST(
				static_cast<size_t>(extract_noise_length(noise)) == noise.size());
		}
	}

	BOOST_AUTO_TEST_CASE(generate_noise_rejects_small_max_len)
	{
		std::random_device rd;
		BOOST_TEST(generate_noise(rd, 15).empty());
		BOOST_TEST(generate_noise(rd, 0).empty());
	}

	BOOST_AUTO_TEST_CASE(extract_noise_length_requires_full_header)
	{
		std::vector<uint8_t> short_data(15, 0);
		BOOST_TEST(extract_noise_length(short_data) == -1);

		std::vector<uint8_t> data(16, 0);
		BOOST_TEST(extract_noise_length(data) == 0);
	}

	BOOST_AUTO_TEST_CASE(compute_key_is_deterministic)
	{
		std::vector<uint8_t> data1{ 1, 2, 3, 4, 5 };
		std::vector<uint8_t> data2{ 1, 2, 3, 4, 6 };
		std::vector<uint8_t> data3{ 1, 2, 3, 4, 5 };

		auto key1 = compute_key(data1);
		auto key3 = compute_key(data3);
		auto key2 = compute_key(data2);

		BOOST_TEST(key1 == key3);
		BOOST_TEST(key1 != key2);
	}

	BOOST_AUTO_TEST_CASE(scramble_stream_key_state)
	{
		scramble_stream stream;
		BOOST_TEST(!stream.is_valid());

		std::array<uint8_t, 16> key{};
		key[0] = 0x42;
		stream.set_key(key);
		BOOST_TEST(stream.is_valid());

		stream.reset();
		BOOST_TEST(!stream.is_valid());

		std::vector<uint8_t> data(32, 0xAB);
		stream.reset(data);
		BOOST_TEST(stream.is_valid());
	}

	BOOST_AUTO_TEST_CASE(scramble_round_trip_with_two_streams)
	{
		std::array<uint8_t, 16> key{};
		for (size_t i = 0; i < key.size(); i++)
			key[i] = static_cast<uint8_t>(i * 7 + 1);

		std::vector<uint8_t> plain(64);
		for (size_t i = 0; i < plain.size(); i++)
			plain[i] = static_cast<uint8_t>(i);

		scramble_stream encoder;
		encoder.set_key(key);
		auto cipher = encoder.scramble(plain);

		BOOST_TEST(cipher.size() == plain.size());
		BOOST_TEST(cipher != plain);

		scramble_stream decoder;
		decoder.set_key(key);
		decoder.scramble(cipher.data(), cipher.size());
		BOOST_TEST(cipher == plain);
	}

	BOOST_AUTO_TEST_CASE(scramble_overloads_agree)
	{
		std::array<uint8_t, 16> key{};
		key[3] = 0x9;

		std::vector<uint8_t> plain(40, 0x5A);

		scramble_stream via_span_stream;
		via_span_stream.set_key(key);
		auto via_span = via_span_stream.scramble(plain);

		scramble_stream via_ptr_stream;
		via_ptr_stream.set_key(key);
		auto via_ptr = plain;
		via_ptr_stream.scramble(via_ptr.data(), via_ptr.size());

		BOOST_TEST(via_span == via_ptr);
	}

	BOOST_AUTO_TEST_CASE(peek_data_does_not_advance_state)
	{
		std::array<uint8_t, 16> key{};
		key[1] = 0x33;

		scramble_stream stream;
		stream.set_key(key);

		std::vector<uint8_t> plain{ 1, 2, 3, 4 };

		auto first = plain;
		stream.peek_data(first);

		auto second = plain;
		stream.peek_data(second);

		BOOST_TEST(first == second);
		BOOST_TEST(first != plain);

		// peek 不改变内部状态, 随后的 scramble 应得到相同结果.
		auto scrambled = stream.scramble(plain);
		BOOST_TEST(scrambled == first);
	}

	BOOST_AUTO_TEST_SUITE_END()

} // namespace

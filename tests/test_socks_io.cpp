//
// test_socks_io.cpp
// ~~~~~~~~~~~~~~~~~

#include "proxy/socks_io.hpp"

#include <boost/test/unit_test.hpp>

#include <array>
#include <cstdint>


namespace {

	BOOST_AUTO_TEST_SUITE(socks_io_suite)

	BOOST_AUTO_TEST_CASE(write_encodes_big_endian_and_advances_pointer)
	{
		std::array<uint8_t, 8> buffer{};
		auto out = buffer.data();

		io_util::write<uint16_t>(0x1234, out);
		io_util::write<uint32_t>(0x89abcdef, out);
		io_util::write<uint16_t>(0x5678, out);

		const std::array<uint8_t, 8> expected = {
			0x12, 0x34, 0x89, 0xab, 0xcd, 0xef, 0x56, 0x78
		};
		BOOST_TEST(buffer == expected,
			boost::test_tools::per_element());
		BOOST_TEST(out == buffer.data() + buffer.size());
	}

	BOOST_AUTO_TEST_CASE(read_decodes_big_endian_and_advances_pointer)
	{
		const std::array<uint8_t, 14> buffer = {
			0x12, 0x34, 0x89, 0xab, 0xcd, 0xef, 0x01, 0x23,
			0x45, 0x67, 0x89, 0xab, 0xcd, 0xef
		};
		auto in = buffer.data();

		BOOST_TEST(io_util::read<uint16_t>(in) == 0x1234);
		BOOST_TEST(io_util::read<uint32_t>(in) == 0x89abcdef);
		BOOST_TEST(io_util::read<uint64_t>(in) == 0x0123456789abcdefULL);
		BOOST_TEST(in == buffer.data() + buffer.size());
	}

	BOOST_AUTO_TEST_CASE(read_write_round_trip)
	{
		std::array<uint8_t, 4> buffer{};
		auto out = buffer.data();
		io_util::write<int32_t>(-123456, out);

		auto in = buffer.data();
		BOOST_TEST(io_util::read<int32_t>(in) == -123456);
		BOOST_TEST(in == buffer.data() + buffer.size());
	}

	BOOST_AUTO_TEST_SUITE_END()

} // namespace

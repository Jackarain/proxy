//
// test_fileop.cpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/fileop.hpp"

#include <boost/test/unit_test.hpp>

#include <string>


namespace {

	namespace fs = boost::filesystem;

	BOOST_AUTO_TEST_SUITE(fileop_suite)

	BOOST_AUTO_TEST_CASE(write_then_read_round_trip)
	{
		auto dir = fs::temp_directory_path() / "proxy_fileop_test";
		auto file = dir / "nested" / "data.bin";

		const std::string payload = "hello fileop";

		auto written = fileop::write(file, std::string_view(payload));
		BOOST_TEST(written == static_cast<std::streamsize>(payload.size()));
		BOOST_TEST(fs::exists(file));

		std::string loaded;
		auto read = fileop::read(file, loaded);
		BOOST_TEST(read == static_cast<std::streamsize>(payload.size()));
		BOOST_TEST(loaded == payload);

		fs::remove_all(dir);
	}

	BOOST_AUTO_TEST_CASE(read_missing_file_reports_failure)
	{
		auto file = fs::temp_directory_path() / "proxy_fileop_missing" / "nope.bin";
		std::string loaded;
		BOOST_TEST(fileop::read(file, loaded) == -1);
	}

	BOOST_AUTO_TEST_CASE(file_hash_matches_sha1)
	{
		auto dir = fs::temp_directory_path() / "proxy_fileop_hash";
		auto file = dir / "content.txt";

		fileop::write(file, std::string_view("hello"));

		boost::system::error_code ec;
		auto hash = fileop::file_hash(file, ec);

		BOOST_TEST(!ec);
		BOOST_TEST(hash == "aaf4c61ddcc5e8a2dabede0f3b482cd9aea9434d");

		fs::remove_all(dir);
	}

	BOOST_AUTO_TEST_CASE(file_hash_missing_file_sets_error)
	{
		auto file = fs::temp_directory_path() / "proxy_fileop_hash" / "absent.txt";

		boost::system::error_code ec;
		auto hash = fileop::file_hash(file, ec);

		BOOST_TEST(!!ec);
		BOOST_TEST(hash.empty());
	}

	BOOST_AUTO_TEST_SUITE_END()

} // namespace

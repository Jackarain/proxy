//
// test_path_util.cpp
// ~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/path_util.hpp"

#include <boost/test/unit_test.hpp>

#include <fstream>
#include <string>


namespace {

	using namespace proxy;

	BOOST_AUTO_TEST_SUITE(path_util_suite)

	BOOST_AUTO_TEST_CASE(path_cat_joins_and_strips_leading_separators)
	{
		BOOST_TEST(path_cat(L"/srv/www", L"/index.html") ==
			fs::path("/srv/www/index.html"));
		BOOST_TEST(path_cat(L"/srv/www/", L"/index.html") ==
			fs::path("/srv/www/index.html"));
		BOOST_TEST(path_cat(L"/srv/www", L"index.html") ==
			fs::path("/srv/www/index.html"));
		BOOST_TEST(path_cat(L"/srv/www", L"///a/b") ==
			fs::path("/srv/www/a/b"));
	}

	BOOST_AUTO_TEST_CASE(make_target_path_normalizes_request_target)
	{
		BOOST_TEST(boost::nowide::narrow(make_target_path("/index.html")) ==
			"/index.html");
		BOOST_TEST(boost::nowide::narrow(make_target_path("index.html")) ==
			"/index.html");
		BOOST_TEST(boost::nowide::narrow(make_target_path("/dir/file.txt")) ==
			"/dir/file.txt");
		// 查询串不属于路径.
		BOOST_TEST(boost::nowide::narrow(make_target_path("/search?q=1")) ==
			"/search");
	}

	BOOST_AUTO_TEST_CASE(make_target_path_rejects_parent_segments)
	{
		BOOST_TEST(make_target_path("/../../etc/passwd").empty());
		BOOST_TEST(make_target_path("/%2e%2e/%2e%2e/etc/passwd").empty());
		BOOST_TEST(make_target_path("/a%2f..%2fetc/passwd").empty());
		BOOST_TEST(make_target_path("/a%5c..%5cetc/passwd").empty());
		BOOST_TEST(make_target_path("/a%00/../../etc/passwd").empty());
	}

	BOOST_AUTO_TEST_CASE(make_real_target_path_combines_doc_and_target)
	{
		BOOST_TEST(make_real_target_path("/srv/www", "/index.html") ==
			fs::path("/srv/www/index.html"));
		BOOST_TEST(make_real_target_path("/srv/www/", "/a/b.css") ==
			fs::path("/srv/www/a/b.css"));
		BOOST_TEST(make_real_target_path(
			"/srv/www", "/../../etc/passwd").empty());
		BOOST_TEST(make_real_target_path(
			"/srv/www", "/%2e%2e/%2e%2e/etc/passwd").empty());
	}

	BOOST_AUTO_TEST_CASE(file_last_write_time_raw_on_missing_file)
	{
		auto [time_c, success, returned_path] = file_last_write_time_raw(
			fs::path("/nonexistent/proxy/path_util_test.txt"));

		BOOST_TEST(!success);
		BOOST_TEST(time_c == 0);
		(void)returned_path;
	}

	BOOST_AUTO_TEST_CASE(file_last_write_time_reports_existing_file)
	{
		auto dir = fs::temp_directory_path() / "proxy_path_util_test";
		fs::create_directories(dir);
		auto file = dir / "stamp.txt";

		{
			std::ofstream ofs(file);
			ofs << "x";
		}

		auto [time_c, success, returned_path] = file_last_write_time_raw(file);
		BOOST_TEST(success);
		BOOST_TEST(time_c > 0);
		(void)returned_path;

		auto [text, unc_path] = file_last_write_time(file);
		BOOST_TEST(!text.empty());
		(void)unc_path;

		fs::remove_all(dir);
	}

	BOOST_AUTO_TEST_SUITE_END()

} // namespace

//
// test_logging.cpp
// ~~~~~~~~~~~~~~~~

#include "proxy/logging.hpp"

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>


namespace {

	namespace fs = std::filesystem;

	class temp_log_dir
	{
	public:
		temp_log_dir()
		{
			auto unique = std::chrono::steady_clock::now()
				.time_since_epoch().count();
			path = fs::temp_directory_path() /
				("proxy_logging_test_" + std::to_string(unique));
		}

		~temp_log_dir()
		{
			std::error_code ignore_ec;
			fs::remove_all(path, ignore_ec);
		}

		fs::path path;
	};

	std::string read_file(const fs::path& path)
	{
		std::ifstream stream(path, std::ios::binary);
		std::ostringstream content;
		content << stream.rdbuf();
		return content.str();
	}

	BOOST_AUTO_TEST_SUITE(logging_suite)

	BOOST_AUTO_TEST_CASE(file_writer_appends_all_written_data)
	{
		temp_log_dir dir;

		{
			xlogger::auto_logger_file__ logger(dir.path.string());
			BOOST_TEST(logger.log_path() ==
				(dir.path / "application.log").string());

			logger.write(1000, "first\n", 6);
			logger.write(1000, "second", 6);
		}

		BOOST_TEST(read_file(dir.path / "application.log") ==
			"first\nsecond");
	}

	BOOST_AUTO_TEST_CASE(file_writer_keeps_data_across_instances)
	{
		temp_log_dir dir;

		{
			xlogger::auto_logger_file__ logger(dir.path.string());
			logger.write(1000, "one", 3);
		}
		{
			xlogger::auto_logger_file__ logger(dir.path.string());
			logger.write(1000, "two", 3);
		}

		BOOST_TEST(read_file(dir.path / "application.log") == "onetwo");
	}

	BOOST_AUTO_TEST_SUITE_END()

} // namespace

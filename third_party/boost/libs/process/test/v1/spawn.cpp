// Copyright (c) 2006, 2007 Julio M. Merino Vidal
// Copyright (c) 2008 Ilya Sokolov, Boris Schaeling
// Copyright (c) 2009 Boris Schaeling
// Copyright (c) 2010 Felipe Tanus, Boris Schaeling
// Copyright (c) 2011, 2012 Jeff Flinn, Boris Schaeling
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MAIN
#define BOOST_TEST_IGNORE_SIGCHLD
#include <boost/test/included/unit_test.hpp>

#include <boost/system/error_code.hpp>

#include <boost/asio.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <boost/process/v1/error.hpp>
#include <boost/process/v1/io.hpp>
#include <boost/process/v1/args.hpp>
#include <boost/process/v1/spawn.hpp>
#include <boost/process/v1/async_pipe.hpp>
#include <boost/process/v1/async.hpp>
#include <system_error>

#include <boost/process/v1/filesystem.hpp>

#include <string>
#include <istream>
#include <cstdlib>
#if defined(BOOST_WINDOWS_API)
#   include <windows.h>
typedef boost::asio::windows::stream_handle pipe_end;
#elif defined(BOOST_POSIX_API)
#   include <sys/wait.h>
#   include <unistd.h>
typedef boost::asio::posix::stream_descriptor pipe_end;
#endif

namespace fs = boost::process::v1::filesystem;
namespace bp = boost::process::v1;

BOOST_AUTO_TEST_CASE(sync_spawn, *boost::unit_test::timeout(5))
{
    using boost::unit_test::framework::master_test_suite;

    bp::ipstream is;
    std::error_code ec;

    bp::spawn(
        master_test_suite().argv[1],
        bp::args+={"test", "--echo-stdout", "hello"},
        bp::std_out > is,
        ec
    ); 

    BOOST_CHECK(!ec);


    std::string s;

    BOOST_TEST_CHECKPOINT("Starting read");
    is >> s;
    BOOST_TEST_CHECKPOINT("Finished read");

    BOOST_CHECK_EQUAL(s, "hello");
}


struct read_handler
{
    boost::asio::streambuf &buffer_;

    read_handler(boost::asio::streambuf &buffer) : buffer_(buffer) {}

    void operator()(const boost::system::error_code &ec, std::size_t size)
    {
        std::istream is(&buffer_);
        std::string line;
        std::getline(is, line);
        BOOST_CHECK(boost::algorithm::starts_with(line, "abc"));
    }
};

BOOST_AUTO_TEST_CASE(async_spawn, *boost::unit_test::timeout(2))
{
    using boost::unit_test::framework::master_test_suite;

    boost::asio::io_context io_context;
    bp::async_pipe p(io_context);

    std::error_code ec;
    bp::spawn(
        master_test_suite().argv[1],
        "test", "--echo-stdout", "abc",
        bp::std_out > p,
        ec
    );
    BOOST_REQUIRE(!ec);

    boost::asio::streambuf buffer;
    boost::asio::async_read_until(p, buffer, '\n',
        read_handler(buffer));

    io_context.run();
}



// Copyright (c) 2022Klemens Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/ignore_unused.hpp>

//[intro
#include <boost/process.hpp>

#include <boost/asio/read.hpp>
#include <boost/asio/readable_pipe.hpp>
#include <boost/system/error_code.hpp>

#include <string>
#include <iostream>

namespace proc   = boost::process;
namespace asio   = boost::asio;


int main()
{
    asio::io_context ctx;
    const auto exe = proc::environment::find_executable("gcc");
    proc::popen c{ctx, exe, {"--version"}};

    std::string line;
    boost::system::error_code ec;

    auto sz = asio::read(c, asio::dynamic_buffer(line), ec);
    assert(ec == asio::error::eof);

    boost::ignore_unused(sz);

    std::cout << "Gcc version: '"  << line << "'" << std::endl;

    c.wait();
    return c.exit_code();
}
//]

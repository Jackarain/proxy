// Copyright (c) 2006, 2007 Julio M. Merino Vidal
// Copyright (c) 2008 Ilya Sokolov, Boris Schaeling
// Copyright (c) 2009 Boris Schaeling
// Copyright (c) 2010 Felipe Tanus, Boris Schaeling
// Copyright (c) 2011, 2012 Jeff Flinn, Boris Schaeling
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/process.hpp>
#include <boost/asio.hpp>

using namespace boost::process;
namespace asio = boost::asio;

int main(int argc, char *argv[])
{
  {
    //tag::readable_pipe[]
    asio::io_context ctx;
    asio::readable_pipe rp{ctx};

    process proc(ctx, "/usr/bin/g++", {"--version"}, process_stdio{{ /* in to default */}, rp, { /* err to default */ }});
    std::string output;

    boost::system::error_code ec;
    asio::read(rp, asio::dynamic_buffer(output), ec);
    assert(!ec || (ec == asio::error::eof));
    proc.wait();
    //end::readable_pipe[]
  }

  {
    //tag::file[]
    asio::io_context ctx;
    // forward both stderr & stdout to stdout of the parent process
    process proc(ctx, "/usr/bin/g++", {"--version"}, process_stdio{{ /* in to default */}, stdout, stdout});
    proc.wait();
    //end::file[]
  }
  {
    //tag::null[]
    asio::io_context ctx;
    // forward stderr to /dev/null or NUL
    process proc(ctx, "/usr/bin/g++", {"--version"}, process_stdio{{ /* in to default */}, {}, nullptr});
    proc.wait();
    //end::null[]
  }
#if defined(BOOST_POSIX_API)
  {
    //tag::native_handle[]
    asio::io_context ctx;
    // ignore stderr
    asio::local::stream_protocol::socket sock{ctx}, other{ctx};
    asio::local::connect_pair(sock, other);
    process proc(ctx, "~/not-a-virus", {}, process_stdio{sock, sock, nullptr});
    proc.wait();
    //end::native_handle[]
  }
#endif
  {
    //tag::popen[]
    asio::io_context ctx;
    boost::process::popen proc(ctx, "/usr/bin/addr2line", {argv[0]});
    asio::write(proc, asio::buffer("main\n"));
    std::string line;
    asio::read_until(proc, asio::dynamic_buffer(line), '\n');
    //end::popen[]
  }
}

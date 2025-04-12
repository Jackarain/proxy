// Copyright (c) 2006, 2007 Julio M. Merino Vidal
// Copyright (c) 2008 Ilya Sokolov, Boris Schaeling
// Copyright (c) 2009 Boris Schaeling
// Copyright (c) 2010 Felipe Tanus, Boris Schaeling
// Copyright (c) 2011, 2012 Jeff Flinn, Boris Schaeling
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/process.hpp>

using namespace boost::process;
namespace asio = boost::asio;

int main()
{
  // tag::start_dir[]
  asio::io_context ctx;
  process ls(ctx.get_executor(), "/ls", {}, process_start_dir("/home"));
  ls.wait();
  // end::start_dir[]
}

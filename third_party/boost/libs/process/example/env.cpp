// Copyright (c) 2006, 2007 Julio M. Merino Vidal
// Copyright (c) 2008 Ilya Sokolov, Boris Schaeling
// Copyright (c) 2009 Boris Schaeling
// Copyright (c) 2010 Felipe Tanus, Boris Schaeling
// Copyright (c) 2011, 2012 Jeff Flinn, Boris Schaeling
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/process.hpp>
#include <unordered_map>

using namespace boost::process;
namespace asio = boost::asio;

int main()
{
  { // tag::current_env[]
    // search in the current environment
    auto exe = environment::find_executable("g++");

    std::unordered_map <environment::key, environment::value> my_env =
        {
            {"SECRET", "THIS_IS_A_TEST"},
            {"PATH",   {"/bin", "/usr/bin"}}
        };

    auto other_exe = environment::find_executable("g++", my_env);
    //end::current_env[]
  }

  {
    // tag::subprocess_env[]
    asio::io_context ctx;
    std::unordered_map<environment::key, environment::value> my_env =
        {
            {"SECRET", "THIS_IS_A_TEST"},
            {"PATH", {"/bin", "/usr/bin"}}
        };
    auto exe = environment::find_executable("g++", my_env);
    process proc(ctx, exe, {"main.cpp"}, process_environment(my_env));
    process pro2(ctx, exe, {"test.cpp"}, process_environment(my_env));
    // end::subprocess_env[]
  }
}

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

  {
    // tag::vector_env[]
    asio::io_context ctx;
    auto c = environment::current();
    // we need to use a value, since windows needs wchar_t.
    std::vector<environment::key_value_pair> my_env{c.begin(), c.end()};
    my_env.push_back("SECRET=THIS_IS_A_TEST");
    auto exe = environment::find_executable("g++", my_env);
    process proc(ctx, exe, {"main.cpp"}, process_environment(my_env));
    // end::vector_env[]

  }

  {
    // tag::map_env[]
    asio::io_context ctx;
    std::unordered_map<environment::key, environment::value> my_env;
    for (const auto & kv : environment::current())
      if (kv.key().string() != "SECRET")
        my_env[kv.key()] = kv.value();

    auto exe = environment::find_executable("g++", my_env);
    process proc(ctx, exe, {"main.cpp"}, process_environment(my_env));
    // end::map_env[]
  }
}

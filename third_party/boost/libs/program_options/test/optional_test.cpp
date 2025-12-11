// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <boost/optional.hpp>

#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
#   include <optional>
#endif
#include <string>

#include "minitest.hpp"

std::vector<std::string> sv(const char* array[], unsigned size)
{
    std::vector<std::string> r;
    for (unsigned i = 0; i < size; ++i)
        r.push_back(array[i]);
    return r;
}

template<template<typename> class OptionalType>
void test_optional()
{
    OptionalType<int> foo, bar, baz;

    po::options_description desc;
    desc.add_options()
        ("foo,f", po::value(&foo), "")
        ("bar,b", po::value(&bar), "")
        ("baz,z", po::value(&baz), "")
        ;

    const char* cmdline1_[] = { "--foo=12", "--bar", "1"};
    std::vector<std::string> cmdline1 = sv(cmdline1_,
                                           sizeof(cmdline1_)/sizeof(const char*));
    po::variables_map vm;
    po::store(po::command_line_parser(cmdline1).options(desc).run(), vm);
    po::notify(vm);

    BOOST_REQUIRE(!!foo);
    BOOST_CHECK(*foo == 12);

    BOOST_REQUIRE(!!bar);
    BOOST_CHECK(*bar == 1);

    BOOST_CHECK(!baz);
}

int main(int, char*[])
{
    test_optional<boost::optional>();
#ifndef BOOST_NO_CXX17_HDR_OPTIONAL
    test_optional<std::optional>();
#endif
    return 0;
}

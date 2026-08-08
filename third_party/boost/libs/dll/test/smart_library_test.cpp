// Copyright 2026 Roman Savchenko
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// For more information, see http://www.boost.org

#include <boost/config.hpp>
#include <boost/predef.h>

#include "../example/b2_workarounds.hpp"

#include <boost/core/lightweight_test.hpp>
#include <boost/filesystem.hpp>

#include <iostream>

#include <boost/dll/smart_library.hpp>

// Test for smart_library construction from empty/unloaded shared_library objects.
int main(int argc, char* argv[])
{
    using namespace boost::dll;
    using namespace boost::dll::experimental;
    boost::dll::fs::path pt = b2_workarounds::first_lib_from_argv(argc, argv);

    BOOST_TEST(!pt.empty());
    std::cout << "Library: " << pt << std::endl;

    {
        shared_library empty_lib;
        BOOST_TEST(!empty_lib.is_loaded());

        // Should not access location() for an unloaded library.
        smart_library sm(empty_lib);
        BOOST_TEST(!sm.is_loaded());
    }

    {
        shared_library lib(pt);
        BOOST_TEST(lib.is_loaded());

        shared_library moved_to  = std::move(lib);
        BOOST_TEST(!lib.is_loaded());
        BOOST_TEST(moved_to.is_loaded());

        // Should not access location() for a moved-from library.
        smart_library sm(std::move(lib));
        BOOST_TEST(!sm.is_loaded());
    }

    {
        shared_library lib(pt);
        BOOST_TEST(lib.is_loaded());

        smart_library sm(lib);
        BOOST_TEST(sm.is_loaded());
    }

    {
        shared_library lib(pt);
        BOOST_TEST(lib.is_loaded());

        smart_library sm(std::move(lib));
        BOOST_TEST(sm.is_loaded());
    }

    return boost::report_errors();
}

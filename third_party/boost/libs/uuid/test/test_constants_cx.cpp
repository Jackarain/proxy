// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/uuid/constants.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <boost/config/pragma_message.hpp>

#if defined(BOOST_NO_CXX14_CONSTEXPR)

BOOST_PRAGMA_MESSAGE("Skipping test because BOOST_NO_CXX14_CONSTEXPR is defined")
int main() {}

#else

using namespace boost::uuids;

constexpr auto u1 = ns::dns();
constexpr auto u2 = ns::url();
constexpr auto u3 = ns::oid();
constexpr auto u4 = ns::x500dn();
constexpr auto u5 = nil_uuid();

int main()
{
    BOOST_TEST_EQ( u1, string_generator()("6ba7b810-9dad-11d1-80b4-00c04fd430c8"));
    BOOST_TEST_EQ( u2, string_generator()("6ba7b811-9dad-11d1-80b4-00c04fd430c8"));
    BOOST_TEST_EQ( u3, string_generator()("6ba7b812-9dad-11d1-80b4-00c04fd430c8"));
    BOOST_TEST_EQ( u4, string_generator()("6ba7b814-9dad-11d1-80b4-00c04fd430c8"));
    BOOST_TEST_EQ( u5, string_generator()("00000000-0000-0000-0000-000000000000"));

    return boost::report_errors();
}

#endif

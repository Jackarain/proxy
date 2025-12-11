/*
Copyright 2023 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/config.hpp>
#if !defined(BOOST_NO_CXX11_CONSTEXPR) && !defined(BOOST_NO_CXX11_DECLTYPE)
#include <boost/core/size.hpp>
#include <boost/core/lightweight_test.hpp>
#include <iterator>

struct range {
    std::size_t size() const {
        return 4;
    }
};

void test_range()
{
    range c;
    BOOST_TEST_EQ(boost::size(c), 4);
}

void test_array()
{
    int a[4];
    BOOST_TEST_EQ(boost::size(a), 4);
}

void test_ambiguity_with_std_size()
{
// Note: This preprocessor check should be equivalent to that in boost/core/size.hpp
#if (defined(__cpp_lib_nonmember_container_access) && (__cpp_lib_nonmember_container_access >= 201411l)) || \
    (defined(_MSC_VER) && (_MSC_VER >= 1900))

    // https://github.com/boostorg/core/issues/206
    range c;
    using std::size;
    using boost::size;
    BOOST_TEST_EQ(size(c), c.size());

#endif
}

int main()
{
    test_range();
    test_array();
    test_ambiguity_with_std_size();
    return boost::report_errors();
}
#else
int main()
{
    return 0;
}
#endif

/*
Copyright 2023 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/config.hpp>
#if !defined(BOOST_NO_CXX11_CONSTEXPR) && !defined(BOOST_NO_CXX11_DECLTYPE)
#include <boost/core/data.hpp>
#include <boost/core/lightweight_test.hpp>
#include <iterator>

class range {
public:
    int* data() {
        return &v_[0];
    }

    const int* data() const {
        return &v_[0];
    }

    std::size_t size() const {
        return 4;
    }

private:
    int v_[4];
};

void test_range()
{
    range c;
    BOOST_TEST_EQ(boost::data(c), c.data());
}

void test_const_range()
{
    const range c = range();
    BOOST_TEST_EQ(boost::data(c), c.data());
}

void test_array()
{
    int a[4];
    BOOST_TEST_EQ(boost::data(a), a);
}

void test_initializer_list()
{
    std::initializer_list<int> l{1, 2, 3, 4};
    BOOST_TEST_EQ(boost::data(l), l.begin());
}

void test_ambiguity_with_std_data()
{
// Note: This preprocessor check should be equivalent to that in boost/core/data.hpp
#if (defined(__cpp_lib_nonmember_container_access) && (__cpp_lib_nonmember_container_access >= 201411l)) || \
    (defined(_MSC_VER) && (_MSC_VER >= 1900))

    // https://github.com/boostorg/core/issues/206
    range c;
    using std::data;
    using boost::data;
    BOOST_TEST_EQ(data(c), c.data());

#endif
}

int main()
{
    test_range();
    test_const_range();
    test_array();
    test_initializer_list();
    test_ambiguity_with_std_data();
    return boost::report_errors();
}
#else
int main()
{
    return 0;
}
#endif

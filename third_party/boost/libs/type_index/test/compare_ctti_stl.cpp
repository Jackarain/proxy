// Copyright Klemens Morgenstern, 2012-2015.
// Copyright 2019-2026 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/type_index/ctti_type_index.hpp>
#include <boost/type_index/stl_type_index.hpp>
#include <boost/core/lightweight_test.hpp>


namespace my_namespace1 {
    class my_class{};
}


namespace my_namespace2 {
    class my_class{};
}

namespace my_namespace3
{
template<typename T, typename U>
struct my_template {};

}

#if !defined( BOOST_NO_RTTI )

template<typename T>
void compare()
{
    using ctti = boost::typeindex::ctti_type_index;
    using stl = boost::typeindex::stl_type_index;
    BOOST_TEST_EQ(
        ctti::type_id<T>().pretty_name(),
        stl::type_id<T>().pretty_name()
    );
}


int main()
{
    compare<void>();
    compare<int>();
    compare<const double&>();

#ifndef _MSC_VER  // may add `class` to the type name
    compare<my_namespace1::my_class>();

    compare<my_namespace3::my_template<
            my_namespace1::my_class,
            my_namespace2::my_class> >();
#endif

    return boost::report_errors();
}

#else

int main()
{
    return 0;
}

#endif


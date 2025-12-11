/*
 *             Copyright Andrey Semashev 2025.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*
 * The test verifies that unqualified calls to data() and size()
 * don't cause ambiguity between std:: and boost:: implementations.
 * The ambiguity used to be caused by ADL bringing boost::data()/size()
 * due to namespace of boost::span and a using-declaration of
 * std::data()/size().
 *
 * https://github.com/boostorg/core/issues/206
 */

#include <boost/config.hpp>

#if !defined(BOOST_NO_CXX11_CONSTEXPR) && !defined(BOOST_NO_CXX11_DECLTYPE)

#include <boost/core/span.hpp>
#include <iterator>

// Note: This preprocessor check should be equivalent to those in boost/core/data.hpp and boost/core/size.hpp
#if (defined(__cpp_lib_nonmember_container_access) && (__cpp_lib_nonmember_container_access >= 201411l)) || \
    (defined(_MSC_VER) && (_MSC_VER >= 1900))

#include <boost/core/data.hpp>
#include <boost/core/size.hpp>

int* test_data_ambiguity(boost::span<int> sp)
{
    using std::data;
    return data(sp);
}

boost::span<int>::size_type test_size_ambiguity(boost::span<int> sp)
{
    using std::size;
    return size(sp);
}

#endif // (defined(__cpp_lib_nonmember_container_access) ...
#endif // !defined(BOOST_NO_CXX11_CONSTEXPR) && !defined(BOOST_NO_CXX11_DECLTYPE)

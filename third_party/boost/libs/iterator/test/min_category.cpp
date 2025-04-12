// Copyright Andrey Semashev 2025.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <boost/iterator/min_category.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <iterator>
#include <type_traits>

using std::is_same;
using boost::iterators::min_category;
using boost::iterators::min_category_t;

int main(int, char*[])
{
    BOOST_TEST_TRAIT_TRUE((is_same<min_category<std::forward_iterator_tag>::type, std::forward_iterator_tag>));
    BOOST_TEST_TRAIT_TRUE((is_same<min_category<std::forward_iterator_tag, std::random_access_iterator_tag>::type, std::forward_iterator_tag>));
    BOOST_TEST_TRAIT_TRUE((is_same<min_category<std::random_access_iterator_tag, std::forward_iterator_tag>::type, std::forward_iterator_tag>));
    BOOST_TEST_TRAIT_TRUE((is_same<min_category<std::random_access_iterator_tag, std::random_access_iterator_tag>::type, std::random_access_iterator_tag>));
    BOOST_TEST_TRAIT_TRUE((is_same<min_category<std::forward_iterator_tag, std::bidirectional_iterator_tag, std::random_access_iterator_tag>::type, std::forward_iterator_tag>));
    BOOST_TEST_TRAIT_TRUE((is_same<min_category<std::random_access_iterator_tag, std::bidirectional_iterator_tag, std::forward_iterator_tag>::type, std::forward_iterator_tag>));

    BOOST_TEST_TRAIT_TRUE((is_same<min_category_t<std::forward_iterator_tag>, std::forward_iterator_tag>));
    BOOST_TEST_TRAIT_TRUE((is_same<min_category_t<std::forward_iterator_tag, std::random_access_iterator_tag>, std::forward_iterator_tag>));
    BOOST_TEST_TRAIT_TRUE((is_same<min_category_t<std::random_access_iterator_tag, std::forward_iterator_tag>, std::forward_iterator_tag>));
    BOOST_TEST_TRAIT_TRUE((is_same<min_category_t<std::random_access_iterator_tag, std::random_access_iterator_tag>, std::random_access_iterator_tag>));
    BOOST_TEST_TRAIT_TRUE((is_same<min_category_t<std::forward_iterator_tag, std::bidirectional_iterator_tag, std::random_access_iterator_tag>, std::forward_iterator_tag>));
    BOOST_TEST_TRAIT_TRUE((is_same<min_category_t<std::random_access_iterator_tag, std::bidirectional_iterator_tag, std::forward_iterator_tag>, std::forward_iterator_tag>));

    return boost::report_errors();
}

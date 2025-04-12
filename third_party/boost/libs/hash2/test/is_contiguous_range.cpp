// Copyright 2017, 2018, 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/digest.hpp>
#include <boost/container_hash/is_contiguous_range.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/array.hpp>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>
#include <string>
#include <vector>
#include <list>
#include <deque>
#include <set>
#include <map>
#include <array>
#include <forward_list>
#include <unordered_set>
#include <unordered_map>

struct X
{
};

int main()
{
    using boost::container_hash::is_contiguous_range;

    BOOST_TEST_TRAIT_FALSE((is_contiguous_range<void>));
    BOOST_TEST_TRAIT_FALSE((is_contiguous_range<void const>));

    BOOST_TEST_TRAIT_FALSE((is_contiguous_range<int>));
    BOOST_TEST_TRAIT_FALSE((is_contiguous_range<int const>));

    BOOST_TEST_TRAIT_FALSE((is_contiguous_range<X>));
    BOOST_TEST_TRAIT_FALSE((is_contiguous_range<X const>));

    BOOST_TEST_TRAIT_FALSE((is_contiguous_range<int[2]>));
    BOOST_TEST_TRAIT_FALSE((is_contiguous_range<int const[2]>));

    BOOST_TEST_TRAIT_TRUE((is_contiguous_range<std::string>));
    BOOST_TEST_TRAIT_TRUE((is_contiguous_range<std::string const>));

    BOOST_TEST_TRAIT_TRUE((is_contiguous_range<std::wstring>));
    BOOST_TEST_TRAIT_TRUE((is_contiguous_range<std::wstring const>));

    BOOST_TEST_TRAIT_TRUE((is_contiguous_range< std::vector<X> >));
    BOOST_TEST_TRAIT_TRUE((is_contiguous_range< std::vector<X> const >));

    BOOST_TEST_TRAIT_FALSE((is_contiguous_range< std::deque<X> >));
    BOOST_TEST_TRAIT_FALSE((is_contiguous_range< std::deque<X> const >));

    BOOST_TEST_TRAIT_FALSE((is_contiguous_range< std::set<int> >));
    BOOST_TEST_TRAIT_FALSE((is_contiguous_range< std::set<int> const >));

    BOOST_TEST_TRAIT_FALSE((is_contiguous_range< std::multiset<int> >));
    BOOST_TEST_TRAIT_FALSE((is_contiguous_range< std::multiset<int> const >));

    BOOST_TEST_TRAIT_FALSE((is_contiguous_range< std::map<int, X> >));
    BOOST_TEST_TRAIT_FALSE((is_contiguous_range< std::map<int, X> const >));

    BOOST_TEST_TRAIT_FALSE((is_contiguous_range< std::multimap<int, X> >));
    BOOST_TEST_TRAIT_FALSE((is_contiguous_range< std::multimap<int, X> const >));

    BOOST_TEST_TRAIT_TRUE((is_contiguous_range< std::array<X, 2> >));
    BOOST_TEST_TRAIT_TRUE((is_contiguous_range< std::array<X, 2> const >));

    BOOST_TEST_TRAIT_TRUE((is_contiguous_range< std::array<X, 0> >));
    BOOST_TEST_TRAIT_TRUE((is_contiguous_range< std::array<X, 0> const >));

    BOOST_TEST_TRAIT_FALSE((is_contiguous_range< std::forward_list<X> >));
    BOOST_TEST_TRAIT_FALSE((is_contiguous_range< std::forward_list<X> const >));

    BOOST_TEST_TRAIT_FALSE((is_contiguous_range< std::unordered_set<int> >));
    BOOST_TEST_TRAIT_FALSE((is_contiguous_range< std::unordered_set<int> const >));

    BOOST_TEST_TRAIT_FALSE((is_contiguous_range< std::unordered_multiset<int> >));
    BOOST_TEST_TRAIT_FALSE((is_contiguous_range< std::unordered_multiset<int> const >));

    BOOST_TEST_TRAIT_FALSE((is_contiguous_range< std::unordered_map<int, X> >));
    BOOST_TEST_TRAIT_FALSE((is_contiguous_range< std::unordered_map<int, X> const >));

    BOOST_TEST_TRAIT_FALSE((is_contiguous_range< std::unordered_multimap<int, X> >));
    BOOST_TEST_TRAIT_FALSE((is_contiguous_range< std::unordered_multimap<int, X> const >));

#if !BOOST_WORKAROUND(BOOST_MSVC, < 1910)

    BOOST_TEST_TRAIT_TRUE((is_contiguous_range< boost::array<X, 2> >));
    BOOST_TEST_TRAIT_TRUE((is_contiguous_range< boost::array<X, 2> const >));

    BOOST_TEST_TRAIT_TRUE((is_contiguous_range< boost::array<X, 0> >));
    BOOST_TEST_TRAIT_TRUE((is_contiguous_range< boost::array<X, 0> const >));

    BOOST_TEST_TRAIT_TRUE((is_contiguous_range< boost::hash2::digest<7> >));
    BOOST_TEST_TRAIT_TRUE((is_contiguous_range< boost::hash2::digest<7> const >));

#endif

    return boost::report_errors();
}

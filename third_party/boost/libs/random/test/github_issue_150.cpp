/*
* Copyright Matt Borland 2025.
 * Distributed under the Boost Software License, Version 1.0. (See
        * accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org for most recent version including documentation.
 *
 * $Id$
 */

#include <boost/random.hpp>
#include <boost/core/lightweight_test.hpp>
#include <random>
#include <cstdint>

int main()
{
    std::mt19937_64 gen;
    boost::random::binomial_distribution<std::uint64_t> dist;

    BOOST_TEST(dist(gen) >= 0);

    return boost::report_errors();
}

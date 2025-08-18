/*
 *             Copyright Andrey Semashev 2025.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */

// This test checks whether the portion of Boost.Graph that is used by Boost.Parameter tests
// is compatible with the current compiler. The headers listed below are used in Boost.Parameter tests.

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/properties.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/adjacency_list.hpp>

int main()
{
}

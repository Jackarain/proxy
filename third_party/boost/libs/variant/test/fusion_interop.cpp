// Copyright (c) 2021-2025 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Test case from https://svn.boost.org/trac10/ticket/8721

#include <boost/variant.hpp>

#include <boost/fusion/include/vector.hpp>

struct emptyList {};

typedef boost::make_recursive_variant
  < emptyList
  , boost::fusion::vector
      < int
      , boost::recursive_variant_
      >
  >::type IntList;

const emptyList el = emptyList();
const IntList nil( el );

IntList cons( int head, IntList tail )
{
  return IntList( boost::fusion::vector<int, IntList>( head, tail ) );
}

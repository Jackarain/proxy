//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2007-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
//
// Compares boost::container::vector against std::vector.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef LONG_BENCH
//#define LONG_BENCH
#endif

#include <vector>
#include <memory>    //std::allocator
#include <boost/container/vector.hpp>

#include "bench_vector_common.hpp"

template<class IntType, class Operation>
void run_containers(runner<IntType, Operation>& r)
{
   //First registered container is the baseline (denominator).
   r.template add< std::vector<IntType, std::allocator<IntType> > >("std::vector");
   r.template add< bc::vector<IntType, std::allocator<IntType> >  >("vector(1.6x)");
   typedef typename bc::vector_options < bc::growth_factor<bc::growth_factor_100> >::type growth_100_t;
   r.template add< bc::vector<IntType, std::allocator<IntType>, growth_100_t> >("vector(2x)");
}

int main()
{
   test_vectors<int>();
   test_vectors<MyInt>();
   return 0;
}

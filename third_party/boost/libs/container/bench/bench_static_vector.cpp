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
// Compares boost::container::static_vector against std::vector and
// boost::container::vector.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef LONG_BENCH
//#define LONG_BENCH
#endif

#include <vector>
#include <memory>    //std::allocator
#include <boost/container/vector.hpp>
#include <boost/container/static_vector.hpp>

#include "bench_vector_common.hpp"

template<class IntType, class Operation>
void run_containers(runner<IntType, Operation>& r)
{
   //First registered container is the baseline (denominator).
   r.template add< std::vector<IntType, std::allocator<IntType> > >("std::vector");
   r.template add< bc::vector<IntType, std::allocator<IntType> >  >("vector");
   //static_vector has a fixed capacity, so it must be sized for the largest
   //element count exercised by the harness.
   r.template add< bc::static_vector<IntType, bench_max_numele>   >("static_vector");
}

int main()
{
   test_vectors<int>();
   return 0;
}

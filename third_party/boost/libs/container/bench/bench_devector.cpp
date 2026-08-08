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
// Compares boost::container::devector against boost::container::deque and
// boost::container::vector.
//
//////////////////////////////////////////////////////////////////////////////

#include <memory>    //std::allocator
#include <boost/container/devector.hpp>
#include <boost/container/deque.hpp>
#include <boost/container/vector.hpp>

#include "bench_vector_common.hpp"

template<class IntType, class Operation>
void run_containers(runner<IntType, Operation>& r)
{
   //First registered container is the baseline (denominator).
   r.template add< bc::vector<IntType, std::allocator<IntType> >   >("vector");
   r.template add< bc::deque<IntType, std::allocator<IntType> >    >("deque");
   r.template add< bc::devector<IntType, std::allocator<IntType> > >("devector");
}

int main()
{
   test_vectors<int>();
   return 0;
}

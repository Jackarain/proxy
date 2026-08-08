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
// Compares boost::container::segtor (reservable and non-reservable) against
// boost::container::deque (reservable and non-reservable).
//
//////////////////////////////////////////////////////////////////////////////

#include <memory>    //std::allocator
#include <boost/container/deque.hpp>
#include <boost/container/segtor.hpp>
#include <boost/container/options.hpp>

#include "bench_vector_common.hpp"

template<class IntType, class Operation>
void run_containers(runner<IntType, Operation>& r)
{
   //First registered container is the baseline (denominator).
   r.template add< bc::deque<IntType, std::allocator<IntType> >   >("deque");
   r.template add< bc::deque<IntType, std::allocator<IntType>,
      typename bc::deque_options<bc::reservable<true> >::type>    >("deque(resv)");
   r.template add< bc::segtor<IntType, std::allocator<IntType> >  >("segtor");
   r.template add< bc::segtor<IntType, std::allocator<IntType>,
      typename bc::segtor_options<bc::reservable<true> >::type>   >("segtor(resv)");
}

int main()
{
   test_vectors<int>();
   return 0;
}

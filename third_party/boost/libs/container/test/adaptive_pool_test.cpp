//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#define BOOST_CONTAINER_ADAPTIVE_NODE_POOL_CHECK_INVARIANTS
#include <boost/container/list.hpp>
#include <boost/container/vector.hpp>
#include <boost/container/adaptive_pool.hpp>
#include "movable_int.hpp"
#include "list_test.hpp"
#include "vector_test.hpp"

using namespace boost::container;

typedef test::overaligned_copyable_int oint_t;

//Alias adaptive pools that allocates ints
typedef adaptive_pool<int> adapt_node_allocator_t;
typedef adaptive_pool<oint_t> adapt_onode_allocator_t;

typedef adaptive_pool<int, 256, 2, 1, 1> adapt_node_allocator_v1_t;
typedef adaptive_pool<oint_t, 256, 2, 1, 1> adapt_onode_allocator_v1_t;

namespace boost {
namespace container {

//Explicit instantiations to catch compilation errors
template class adaptive_pool<int>;
template class adaptive_pool<oint_t>;

}} //boost::container

//Alias list types
typedef boost::container::list<int, adapt_node_allocator_t>       MyList;
typedef boost::container::list<oint_t, adapt_onode_allocator_t>   MyOList;

typedef boost::container::list<int, adapt_node_allocator_v1_t>       MyListV1;
typedef boost::container::list<oint_t, adapt_onode_allocator_v1_t>   MyOListV1;

//Alias vector types
typedef boost::container::vector<int, adapt_node_allocator_t>     MyVector;
typedef boost::container::vector<oint_t, adapt_onode_allocator_t> MyOVector;

typedef boost::container::vector<int, adapt_node_allocator_v1_t>     MyVectorV1;
typedef boost::container::vector<oint_t, adapt_onode_allocator_v1_t> MyOVectorV1;

int main ()
{
   if(test::list_test<MyList, true>())
      return 1;
   if(test::list_test<MyOList, true>())
      return 1;
   if(test::list_test<MyListV1, true>())
      return 1;
   if(test::list_test<MyOListV1, true>())
      return 1;

   if(test::vector_test<MyVector>())
      return 1;
   if(test::vector_test<MyOVector>())
      return 1;
   if(test::vector_test<MyVectorV1>())
      return 1;
   if(test::vector_test<MyOVectorV1>())
      return 1;

   return 0;
}

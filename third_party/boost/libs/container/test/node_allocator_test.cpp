//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/container/list.hpp>
#include <boost/container/vector.hpp>
#include <boost/container/node_allocator.hpp>
#include "movable_int.hpp"
#include "list_test.hpp"
#include "vector_test.hpp"

using namespace boost::container;

typedef test::overaligned_copyable_int oint_t;

//Alias adaptive pools that allocates ints
typedef node_allocator<int>      node_allocator_t;
typedef node_allocator<oint_t>   onode_allocator_t;

typedef node_allocator<int, 256, 1>    node_allocator_v1_t;
typedef node_allocator<oint_t, 256, 1> onode_allocator_v1_t;

namespace boost {
namespace container {

//Explicit instantiations to catch compilation errors
template class node_allocator<int>;
template class node_allocator<oint_t>;

}} //boost::container

//Alias list types
typedef boost::container::list<int, node_allocator_t>          MyList;
typedef boost::container::list<oint_t, onode_allocator_t>      MyOList;

typedef boost::container::list<int, node_allocator_v1_t>       MyListV1;
typedef boost::container::list<oint_t, onode_allocator_v1_t>   MyOListV1;

//Alias vector types
typedef boost::container::vector<int, node_allocator_t>        MyVector;
typedef boost::container::vector<oint_t, onode_allocator_t>    MyOVector;

typedef boost::container::vector<int, node_allocator_v1_t>     MyVectorV1;
typedef boost::container::vector<oint_t, onode_allocator_v1_t> MyOVectorV1;

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

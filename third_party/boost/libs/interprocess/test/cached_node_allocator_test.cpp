//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2004-2012. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/interprocess for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/container/list.hpp>
#include <boost/container/vector.hpp>
#include <boost/interprocess/allocators/cached_node_allocator.hpp>
#include "movable_int.hpp"
#include "list_test.hpp"
#include "vector_test.hpp"

using namespace boost::interprocess;

typedef test::overaligned_copyable_int oint_t;

//Alias an integer node allocator type
typedef cached_node_allocator
   <int, managed_shared_memory::segment_manager>
   cached_node_allocator_t;
typedef ipcdetail::cached_node_allocator_v1
   <int, managed_shared_memory::segment_manager>
   cached_node_allocator_v1_t;

typedef cached_node_allocator
   < oint_t, managed_shared_memory::segment_manager> shmem_onode_allocator_t;
typedef ipcdetail::cached_node_allocator_v1
   < oint_t, managed_shared_memory::segment_manager> shmem_onode_allocator_v1_t;

namespace boost {
namespace interprocess {

//Explicit instantiations to catch compilation errors
template class cached_node_allocator<int, managed_shared_memory::segment_manager>;
template class cached_node_allocator<oint_t, managed_shared_memory::segment_manager>;
template class cached_node_allocator<void, managed_shared_memory::segment_manager>;

namespace ipcdetail {

template class ipcdetail::cached_node_allocator_v1<int, managed_shared_memory::segment_manager>;
template class ipcdetail::cached_node_allocator_v1<oint_t, managed_shared_memory::segment_manager>;
template class ipcdetail::cached_node_allocator_v1<void, managed_shared_memory::segment_manager>;

}}}

//Alias list types
typedef boost::container::list<int, cached_node_allocator_t>    MyShmList;
typedef boost::container::list<int, cached_node_allocator_v1_t> MyShmListV1;
typedef boost::container::list<oint_t, shmem_onode_allocator_t>    MyOShmList;
typedef boost::container::list<oint_t, shmem_onode_allocator_v1_t> MyOShmListV1;

//Alias vector types
typedef boost::container::vector<int, cached_node_allocator_t>    MyShmVector;
typedef boost::container::vector<int, cached_node_allocator_v1_t> MyShmVectorV1;
typedef boost::container::vector<oint_t, shmem_onode_allocator_t>    MyOShmVector;
typedef boost::container::vector<oint_t, shmem_onode_allocator_v1_t> MyOShmVectorV1;

int main ()
{
   if(test::list_test<managed_shared_memory, MyShmList, true>())
      return 1;
   if(test::list_test<managed_shared_memory, MyShmListV1, true>())
      return 1;

   if(test::list_test<managed_shared_memory, MyOShmList, true>())
      return 1;
   if(test::list_test<managed_shared_memory, MyOShmListV1, true>())
      return 1;

   if(test::vector_test<managed_shared_memory, MyShmVector>())
      return 1;
   if(test::vector_test<managed_shared_memory, MyShmVectorV1>())
      return 1;

   if(test::vector_test<managed_shared_memory, MyOShmVector>())
      return 1;
   if(test::vector_test<managed_shared_memory, MyOShmVectorV1>())
      return 1;

   return 0;
}

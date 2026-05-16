//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2007-2012. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/interprocess for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#define BOOST_CONTAINER_ADAPTIVE_NODE_POOL_CHECK_INVARIANTS
#include "node_pool_test.hpp"
#include <boost/interprocess/allocators/detail/adaptive_node_pool.hpp>
#include <vector>

using namespace boost::interprocess;
typedef managed_shared_memory::segment_manager segment_manager_t;

int main ()
{
   {  //Private, normal alignment, small data
      typedef ipcdetail::private_adaptive_node_pool
         <segment_manager_t, 1u, 64, 64, 5, 0> node_pool_t;

      if (!test::test_all_node_pool<node_pool_t>())
         return 1;
   }
   {  //Private, small alignment, small data
      typedef ipcdetail::private_adaptive_node_pool
         <segment_manager_t, 1u, 64, 64, 5, 2> node_pool_t;

      if (!test::test_all_node_pool<node_pool_t>())
         return 1;
   }
   {  //Private, normal alignment
      typedef ipcdetail::private_adaptive_node_pool
         <segment_manager_t, sizeof(void*), 64, 64, 5, 0> node_pool_t;

      if (!test::test_all_node_pool<node_pool_t>())
         return 1;
   }
   {  //Private, overaligned
      typedef ipcdetail::private_adaptive_node_pool
         <segment_manager_t, sizeof(void*), 64, 64, 5, sizeof(void*)*4U> node_pool_t;

      if (!test::test_all_node_pool<node_pool_t>())
         return 1;
   }

   {  //Shared, normal alignment
      typedef ipcdetail::shared_adaptive_node_pool
         <segment_manager_t, 4u, 64, 64, 5, 0> node_pool_t;

      if (!test::test_all_node_pool<node_pool_t>())
         return 1;
   }
   {  //Shared, overaligned
      typedef ipcdetail::shared_adaptive_node_pool
         <segment_manager_t, sizeof(void*), 64, 64, 5, sizeof(void*) * 4u> node_pool_t;

      if (!test::test_all_node_pool<node_pool_t>())
         return 1;
   }

   return 0;
}

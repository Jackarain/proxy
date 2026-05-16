//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2004-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/interprocess for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <map>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include "print_container.hpp"
#include "movable_int.hpp"
#include "map_test.hpp"
#include "emplace_test.hpp"

/////////////////////////////////////////////////////////////////
//
//  This example repeats the same operations with std::map and
//  shmem_map using the node allocator
//  and compares the values of both containers
//
/////////////////////////////////////////////////////////////////

using namespace boost::interprocess;

//Customize managed_shared_memory class
typedef basic_managed_shared_memory
   <char,
    //simple_seq_fit<mutex_family>,
    rbtree_best_fit<mutex_family>,
    iset_index
   > my_managed_shared_memory;

//Alias allocator type
typedef allocator<std::pair<int, int>, my_managed_shared_memory::segment_manager>
   shmem_pair_allocator_t;
typedef allocator<std::pair<test::movable_int, test::movable_int>, my_managed_shared_memory::segment_manager>
   shmem_movable_pair_allocator_t;

typedef allocator<std::pair<test::movable_and_copyable_int, test::movable_and_copyable_int>, my_managed_shared_memory::segment_manager>
   shmem_move_copy_pair_allocator_t;

//Alias map types
typedef std::map<int, int>                                              MyStdMap;
typedef std::multimap<int, int>                                         MyStdMultiMap;

typedef boost::container::flat_map<int, int, std::less<int>, shmem_pair_allocator_t>      MyShmMap;
typedef boost::container::flat_multimap<int, int, std::less<int>, shmem_pair_allocator_t> MyShmMultiMap;

typedef boost::container::flat_map<test::movable_int, test::movable_int
                ,std::less<test::movable_int>
                ,shmem_movable_pair_allocator_t>                        MyMovableShmMap;
typedef boost::container::flat_multimap<test::movable_int, test::movable_int
                ,std::less<test::movable_int>
                ,shmem_movable_pair_allocator_t>                        MyMovableShmMultiMap;

typedef boost::container::flat_map<test::movable_and_copyable_int, test::movable_and_copyable_int
                ,std::less<test::movable_and_copyable_int>
                ,shmem_move_copy_pair_allocator_t>                        MyMoveCopyShmMap;
typedef boost::container::flat_multimap<test::movable_and_copyable_int, test::movable_and_copyable_int
                ,std::less<test::movable_and_copyable_int>
                ,shmem_move_copy_pair_allocator_t>                        MyMoveCopyShmMultiMap;

int main()
{
   using namespace boost::interprocess::test;

   if (0 != map_test<my_managed_shared_memory
                  ,MyShmMap
                  ,MyStdMap
                  ,MyShmMultiMap
                  ,MyStdMultiMap>()){
      std::cout << "Error in map_test<MyShmMap>" << std::endl;
      return 1;
   }

   if (0 != map_test_copyable<my_managed_shared_memory
                  ,MyShmMap
                  ,MyStdMap
                  ,MyShmMultiMap
                  ,MyStdMultiMap>()){
      std::cout << "Error in map_test<MyShmMap>" << std::endl;
      return 1;
   }

//MSVC 14.5 (2026 ICEs when trying to compile move-only types in flat_map)
#if defined(BOOST_MSVC) && (BOOST_MSVC == 1960)
   if (0 != map_test<my_managed_shared_memory
                  ,MyMovableShmMap
                  ,MyStdMap
                  ,MyMovableShmMultiMap
                  ,MyStdMultiMap>()){
      return 1;
   }

   if (0 != map_test<my_managed_shared_memory
                  ,MyMoveCopyShmMap
                  ,MyStdMap
                  ,MyMoveCopyShmMultiMap
                  ,MyStdMultiMap>()){
      std::cout << "Error in map_test<MyMoveCopyShmMap>" << std::endl;
      return 1;
   }

   if (0 != map_test<my_managed_shared_memory
                  ,MyCopyShmSet
                  ,MyStdSet
                  ,MyCopyShmMultiSet
                  ,MyStdMultiSet>()){
      std::cout << "Error in set_test<MyCopyShmSet>" << std::endl;
      return 1;
   }
#endif
   return 0;
}

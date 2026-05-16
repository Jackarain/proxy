//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2025. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/interprocess for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/container/vector.hpp>
#include <boost/interprocess/managed_heap_memory.hpp>
#include <cstdio>
#include <string>
#include "get_process_id_name.hpp"

using namespace boost::interprocess;

template <class CharT>
struct filename_traits;

template <>
struct filename_traits<char>
{

   static const char* get()
   {  return filename.c_str();  }

   static std::string filename;
};

std::string filename_traits<char>::filename = get_filename();


#ifdef BOOST_INTERPROCESS_WCHAR_NAMED_RESOURCES

template <>
struct filename_traits<wchar_t>
{

   static const wchar_t* get()
   {  return filename.c_str();  }

   static std::wstring filename;
};

std::wstring filename_traits<wchar_t>::filename = get_wfilename();

#endif   //#ifdef BOOST_INTERPROCESS_WCHAR_NAMED_RESOURCES

template<class CharT>
int test_managed_heap_memory()
{
   const int MemSize          = 65536*10;

   //STL compatible allocator object for heap
   typedef allocator<int, managed_heap_memory::segment_manager>
      allocator_int_t;
   //A vector that uses that allocator
   typedef boost::container::vector<int, allocator_int_t> MyVect;

   {
      const int max              = 100;
      void *array[std::size_t(max)];
      //Named allocate capable shared memory allocator
      managed_heap_memory mheap(MemSize);

      std::size_t i;
      //Let's allocate some memory
      for(i = 0; i < max; ++i){
         array[std::ptrdiff_t(i)] = mheap.allocate(i+1u);
      }

      //Deallocate allocated memory
      for(i = 0; i < max; ++i){
         mheap.deallocate(array[std::ptrdiff_t(i)]);
      }

      //Construct the STL-like allocator with the segment manager
      const allocator_int_t myallocator (mheap.get_segment_manager());

      //Construct vector
      MyVect *mheap_vect = mheap.construct<MyVect> ("MyVector") (myallocator);

      //Test that vector can be found via name
      if(mheap_vect != mheap.find<MyVect>("MyVector").first)
         return -1;

      //Destroy and check it is not present
      mheap.destroy<MyVect> ("MyVector");
      mheap_vect = 0;
      if(0 != mheap.find<MyVect>("MyVector").first)
         return -1;

      //Construct a vector in the heap
      mheap_vect = mheap.construct<MyVect> ("MyVector") (myallocator);

      managed_heap_memory::size_type old_free_memory = mheap.get_free_memory();

      //Now grow the file
      mheap.grow(MemSize);

      //Check vector is still there
      mheap_vect = mheap.find<MyVect>("MyVector").first;
      if(!mheap_vect)
         return -1;

      if(mheap.get_size() != (MemSize*2))
         return -1;
      if(mheap.get_free_memory() <= old_free_memory)
         return -1;

      //Destroy and check it is not present
      mheap.destroy_ptr(mheap_vect);
      mheap_vect = 0;
      if(0 != mheap.find<MyVect>("MyVector").first)
         return -1;

      {
         //Now test move semantics
         managed_heap_memory original(1024u);
         managed_heap_memory move_ctor(boost::move(original));
         managed_heap_memory move_assign;
         move_assign = boost::move(move_ctor);
         move_assign.swap(original);
      }
   }

   return 0;
}

int main ()
{
   int r;
   r = test_managed_heap_memory<char>();
   if(r) return r;
   #ifdef BOOST_INTERPROCESS_WCHAR_NAMED_RESOURCES
   r = test_managed_heap_memory<wchar_t>();
   if(r) return r;
   #endif
   return 0;
}

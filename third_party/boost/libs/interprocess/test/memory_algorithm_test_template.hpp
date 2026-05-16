//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2006-2012. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/interprocess for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_INTERPROCESS_TEST_MEMORY_ALGORITHM_TEST_TEMPLATE_HEADER
#define BOOST_INTERPROCESS_TEST_MEMORY_ALGORITHM_TEST_TEMPLATE_HEADER

#include <boost/interprocess/detail/config_begin.hpp>

#include <boost/container/vector.hpp>

#include <vector>
#include <iostream>
#include <new> //std::nothrow
#include <cstring>   //std::memset
#include <typeinfo>

namespace {

bool is_aligned(void *ptr, std::size_t alignment)
{
   return ((std::size_t)ptr & (alignment - 1)) == 0;
}

}  //namespace {

namespace boost { namespace interprocess { namespace test {

enum deallocation_type { DirectDeallocation, InverseDeallocation, MixedDeallocation, EndDeallocationType };

//This test allocates until there is no more memory
//and after that deallocates all in the inverse order
template<class SegMngr>
bool test_allocation(SegMngr &sm)
{
   for( deallocation_type t = DirectDeallocation
      ; t != EndDeallocationType
      ; t = (deallocation_type)((int)t + 1)){
      std::vector<void*> buffers;
      typename SegMngr::size_type free_memory = sm.get_free_memory();

      for(std::size_t i = 0; true; ++i){
         void *ptr = sm.allocate(i, std::nothrow);
         if(!ptr)
            break;
         std::size_t size = sm.size(ptr);
         std::memset(ptr, 0, size);
         buffers.push_back(ptr);
      }

      switch(t){
         case DirectDeallocation:
         {
            for(std::size_t j = 0, max = buffers.size()
               ;j < max
               ;++j){
               sm.deallocate(buffers[j]);
            }
         }
         break;
         case InverseDeallocation:
         {
            for(std::size_t j = buffers.size()
               ;j--
               ;){
               sm.deallocate(buffers[j]);
            }
         }
         break;
         case MixedDeallocation:
         {
            for(std::size_t j = 0, max = buffers.size()
               ;j < max
               ;++j){
               std::size_t pos = (j%4)*(buffers.size())/4;
               sm.deallocate(buffers[pos]);
               buffers.erase(buffers.begin()+std::ptrdiff_t(pos));
            }
         }
         break;
         default:
         break;
      }
      bool ok = free_memory == sm.get_free_memory() &&
               sm.all_memory_deallocated() && sm.check_sanity();
      if(!ok)  return ok;
   }
   return true;
}

//This test allocates until there is no more memory
//and after that tries to shrink all the buffers to the
//half of the original size
template<class SegMngr>
bool test_allocation_shrink(SegMngr &sm)
{
   std::vector<void*> buffers;
   std::vector<std::size_t> sizes;

   //Allocate buffers with extra memory
   for(std::size_t i = 0; true; ++i){
      void *ptr = sm.allocate(i*2, std::nothrow);
      if(!ptr)
         break;
      std::size_t size = sm.size(ptr);
      std::memset(ptr, 0, size);
      buffers.push_back(ptr);
      sizes.push_back(size);
   }

   //Now shrink to half
   for(std::size_t i = 0, max = buffers.size()
      ;i < max
      ; ++i){
      typename SegMngr::size_type received_size;
      char *reuse = static_cast<char*>(buffers[i]);
      if(sm.template allocation_command<char>
         ( boost::interprocess::shrink_in_place | boost::interprocess::nothrow_allocation, sizes[i]
         , received_size = i, reuse)){
         if(received_size > sizes[i]){
            return false;
         }
         if(received_size < std::size_t(i)){
            return false;
         }
         const std::size_t sz = sm.size(buffers[i]);
         if (received_size != sz) {
            return false;
         }

         std::memset(buffers[i], 0, sz);
      }
   }

   //Deallocate it in non sequential order
   for(std::size_t j = 0, max = buffers.size()
      ;j < max
      ;++j){
      std::size_t pos = (j%4)*(buffers.size())/4;
      sm.deallocate(buffers[pos]);
      buffers.erase(buffers.begin()+std::ptrdiff_t(pos));
   }

   return sm.all_memory_deallocated() && sm.check_sanity();
}

//This test allocates until there is no more memory
//and after that tries to expand all the buffers to
//avoid the wasted internal fragmentation
template<class SegMngr>
bool test_allocation_expand(SegMngr &sm)
{
   std::vector<void*> buffers;

   //We will repeat this test for different sized elements and alignments
   for(std::size_t al = 1; al <= SegMngr::MemAlignment*32u; al *= 2u) {
      //Allocate buffers with extra memory
      for(std::size_t i = 0; true; ++i){
         void *ptr = al > SegMngr::MemAlignment
                   ? sm.allocate_aligned(i, al, std::nothrow)
                   : sm.allocate(i, std::nothrow);
         if(!ptr)
            break;
         std::size_t size = sm.size(ptr);
         std::memset(ptr, 0, size);
         buffers.push_back(ptr);
      }

      //Now try to expand to the double of the size
      for(std::size_t i = 0, max = buffers.size()
         ;i < max
         ;++i){
         typename SegMngr::size_type received_size;
         std::size_t min_size = i+1;
         std::size_t preferred_size = i*2;
         preferred_size = min_size > preferred_size ? min_size : preferred_size;

         char *reuse = static_cast<char*>(buffers[i]);
         while(sm.template allocation_command<char>
            ( boost::interprocess::expand_fwd | boost::interprocess::nothrow_allocation, min_size
            , received_size = preferred_size, reuse)){
            //Check received size is bigger than minimum
            if(received_size < min_size){
               return false;
            }
            //Now, try to expand further
            min_size       = received_size+1;
            preferred_size = min_size*2;
         }
      }

      //Deallocate it in non sequential order
      for(std::size_t j = 0, max = buffers.size()
         ;j < max
         ;++j){
         std::size_t pos = (j%4)*(buffers.size())/4;
         sm.deallocate(buffers[pos]);
         buffers.erase(buffers.begin()+std::ptrdiff_t(pos));
      }
   }

   return sm.all_memory_deallocated() && sm.check_sanity();
}

//This test allocates until there is no more memory
//and after that tries to expand all the buffers to
//avoid the wasted internal fragmentation
template<class SegMngr>
bool test_allocation_shrink_and_expand(SegMngr &sm)
{
   std::vector<void*> buffers;
   std::vector<typename SegMngr::size_type> received_sizes;
   std::vector<bool>        size_reduced;

   //Allocate buffers wand store received sizes
   for(std::size_t i = 0; true; ++i){
      typename SegMngr::size_type received_size;
      char *reuse = 0;
      void *ptr = sm.template allocation_command<char>
         ( boost::interprocess::allocate_new | boost::interprocess::nothrow_allocation, i, received_size = i*2, reuse);
      if(!ptr){
         ptr = sm.template allocation_command<char>
            ( boost::interprocess::allocate_new | boost::interprocess::nothrow_allocation, 1, received_size = i*2, reuse);
         if(!ptr)
            break;
      }
      buffers.push_back(ptr);
      received_sizes.push_back(received_size);
   }

   //Now shrink to half
   for(std::size_t i = 0, max = buffers.size()
      ; i < max
      ; ++i){
      typename SegMngr::size_type received_size;
      char *reuse = static_cast<char*>(buffers[i]);
      if(sm.template allocation_command<char>
         ( boost::interprocess::shrink_in_place | boost::interprocess::nothrow_allocation, received_sizes[i]
         , received_size = i, reuse)){
         if(received_size > std::size_t(received_sizes[i])){
            return false;
         }
         if(received_size < std::size_t(i)){
            return false;
         }
         size_reduced.push_back(received_size != received_sizes[i]);
      }
   }

   //Now try to expand to the original size
   for(std::size_t i = 0, max = buffers.size()
      ;i < max
      ;++i){
      typename SegMngr::size_type received_size;
      std::size_t request_size = received_sizes[i];
      char *reuse = static_cast<char*>(buffers[i]);
      if(sm.template allocation_command<char>
         ( boost::interprocess::expand_fwd | boost::interprocess::nothrow_allocation, request_size
         , received_size = request_size, reuse)){
         if(received_size != received_sizes[i]){
            return false;
         }
      }
      else{
         return false;
      }
   }

   //Deallocate it in non sequential order
   for(std::size_t j = 0, max = buffers.size()
      ;j < max
      ;++j){
      std::size_t pos = (j%4)*(buffers.size())/4;
      sm.deallocate(buffers[pos]);
      buffers.erase(buffers.begin()+std::ptrdiff_t(pos));
   }

   return sm.all_memory_deallocated() && sm.check_sanity();
}

//This test allocates until there is no more memory
//and after that deallocates the odd buffers to
//make room for expansions. The expansion will probably
//success since the deallocation left room for that.
template<class SegMngr>
bool test_allocation_deallocation_expand(SegMngr &sm)
{
   std::vector<void*> buffers;

   //Allocate buffers with extra memory
   for(std::size_t i = 0; true; ++i){
      void *ptr = sm.allocate(i, std::nothrow);
      if(!ptr)
         break;
      std::size_t size = sm.size(ptr);
      std::memset(ptr, 0, size);
      buffers.push_back(ptr);
   }

   //Now deallocate the half of the blocks
   //so expand maybe can merge new free blocks
   for(std::size_t i = 0, max = buffers.size()
      ;i < max
      ;++i){
      if(i%2){
         sm.deallocate(buffers[i]);
         buffers[i] = 0;
      }
   }

   //Now try to expand to the double of the size
   for(std::size_t i = 0, max = buffers.size()
      ;i < max
      ;++i){
      //
      if(buffers[i]){
         typename SegMngr::size_type received_size;
         std::size_t min_size = i+1;
         std::size_t preferred_size = i*2;
         preferred_size = min_size > preferred_size ? min_size : preferred_size;

         char *reuse = static_cast<char*>(buffers[i]);
         while(sm.template allocation_command<char>
            ( boost::interprocess::expand_fwd | boost::interprocess::nothrow_allocation, min_size
            , received_size = preferred_size, reuse)){
            //Check received size is bigger than minimum
            if(received_size < min_size){
               return false;
            }
            //Now, try to expand further
            min_size       = received_size+1;
            preferred_size = min_size*2;
         }
      }
   }

   //Now erase null values from the vector
   buffers.erase( std::remove(buffers.begin(), buffers.end(), static_cast<void*>(0))
                , buffers.end());

   //Deallocate it in non sequential order
   for(std::size_t j = 0, max = buffers.size()
      ;j < max
      ;++j){
      std::size_t pos = (j%4)*(buffers.size())/4;
      sm.deallocate(buffers[pos]);
      buffers.erase(buffers.begin()+std::ptrdiff_t(pos));
   }

   return sm.all_memory_deallocated() && sm.check_sanity();
}

//This test allocates until there is no more memory
//and after that deallocates all except the last.
//If the allocation algorithm is a bottom-up algorithm
//the last buffer will be in the end of the segment.
//Then the test will start expanding backwards, until
//the buffer fills all the memory
template<class SegMngr>
bool test_allocation_with_reuse(SegMngr &sm)
{
   //We will repeat this test for different sized elements and alignments
   for(std::size_t al = 1; al <= SegMngr::MemAlignment*32u; al *= 2u)
   for(std::size_t sizeof_object = 1; sizeof_object < 20u; ++sizeof_object){
      std::vector<void*> buffers;

      //Allocate buffers with extra memory
      for(std::size_t i = 0; true; ++i){
         void *ptr = al > SegMngr::MemAlignment
                   ? sm.allocate_aligned(i*sizeof_object, al, std::nothrow)
                   : sm.allocate(i*sizeof_object, std::nothrow);
         if(!ptr)
            break;
         std::size_t size = sm.size(ptr);
         std::memset(ptr, 0, size);
         buffers.push_back(ptr);
      }

      //Now deallocate all except the latest
      //Now try to expand to the double of the sizeof_object
      for(std::size_t i = 0, max = buffers.size() - 1
         ;i < max
         ;++i){
         sm.deallocate(buffers[i]);
      }

      //Save the unique buffer and clear vector
      void *ptr = buffers.back();
      buffers.clear();

      //Now allocate with reuse
      typename SegMngr::size_type received_size = 0;
      for(std::size_t i = 0; true; ++i){
         std::size_t min_size = (received_size + 1);
         std::size_t prf_size = (received_size + (i+1)*2);
         void *reuse = ptr;
         void *ret = sm.allocation_command
            ( boost::interprocess::expand_bwd | boost::interprocess::nothrow_allocation, min_size
            , received_size = prf_size, reuse, sizeof_object, al);
         if(!ret)
            break;
         if(!is_aligned(ret, al))
            return false;
         //If we have memory, this must be a buffer reuse
         if(!reuse)
            return 1;
         if(received_size < min_size)
            return 1;
         ptr = ret;
      }
      //There is only a single block so deallocate it
      sm.deallocate(ptr);

      if(!sm.all_memory_deallocated() || !sm.check_sanity())
         return false;
   }
   return true;
}


//This test allocates memory with different alignments
//and checks returned memory is aligned.
template<class SegMngr>
bool test_aligned_allocation(SegMngr &sm)
{
   //Allocate aligned buffers in a loop
   //and then deallocate it
   bool continue_loop = true;
   for(std::size_t i = 1; continue_loop; i <<= 1){
      for(std::size_t j = 1; true; j <<= 1){
         void *ptr = sm.allocate_aligned(i-1, j, std::nothrow);
         if(!ptr){
            if(j == 1)
               continue_loop = false;
            break;
         }

         if(!is_aligned(ptr, j))
            return false;
         std::memset(ptr, 0xFF, i - 1);
         sm.deallocate(ptr);
         if(!sm.all_memory_deallocated() || !sm.check_sanity()){
            return false;
         }
      }
   }

   return sm.all_memory_deallocated() && sm.check_sanity();
}

//This test allocates memory with different alignments
//and checks returned memory is aligned.
template<class SegMngr>
bool test_continuous_aligned_allocation(SegMngr &sm)
{
   std::vector<void*> buffers;
   //Allocate aligned buffers in a loop
   //and then deallocate it
   bool continue_loop = true;
   for(unsigned i = 1; continue_loop && i; i <<= 1){
      for(std::size_t j = 1; j; j <<= 1){
         for(bool any_allocated = false; 1;){
            void *ptr = sm.allocate_aligned(i-1, j, std::nothrow);
            buffers.push_back(ptr);
            if(!ptr){
               if(j == 1 && !any_allocated){
                  continue_loop = false;
               }
               break;
            }
            else{
               any_allocated = true;
            }

            if(!is_aligned(ptr, j))
               return false;
         }
         //Deallocate all
         for(std::size_t k = buffers.size(); k--;){
            sm.deallocate(buffers[k]);
         }
         buffers.clear();
         if(!sm.all_memory_deallocated() && sm.check_sanity())
            return false;
         if(!continue_loop)
            break;
      }
   }

   return sm.all_memory_deallocated() && sm.check_sanity();
}

//This test allocates memory, writes it with a non-zero value and
//tests zero_free_memory initializes to zero for the next allocation
template<class SegMngr>
bool test_clear_free_memory(SegMngr &sm)
{
   std::vector<void*> buffers;

   //Allocate memory
   for(std::size_t i = 0; true; ++i){
      void *ptr = sm.allocate(i, std::nothrow);
      if(!ptr)
         break;
      std::size_t size = sm.size(ptr);
      std::memset(ptr, 1, size);
      buffers.push_back(ptr);
   }

   //Mark it
   for(std::size_t i = 0, max = buffers.size(); i < max; ++i){
      std::memset(buffers[i], 1, i);
   }

   //Deallocate all
   for(std::size_t j = buffers.size()
      ;j--
      ;){
      sm.deallocate(buffers[j]);
   }
   buffers.clear();

   if(!sm.all_memory_deallocated() && sm.check_sanity())
      return false;

   //Now clear all free memory
   sm.zero_free_memory();

   if(!sm.all_memory_deallocated() && sm.check_sanity())
      return false;

   //Now test all allocated memory is zero
   //Allocate memory
   const char *first_addr = 0;
   for(std::size_t i = 0; true; ++i){
      void *ptr = sm.allocate(i, std::nothrow);
      if(!ptr)
         break;
      if(i == 0){
         first_addr = (char*)ptr;
      }
      std::size_t memsize = sm.size(ptr);
      buffers.push_back(ptr);

      for(std::size_t j = 0; j < memsize; ++j){
         if(static_cast<char*>((char*)ptr)[j]){
            std::cout << "Zero memory test failed. in buffer " << i
                      << " byte " << j << " first address " << (const void*) first_addr
                      << " offset " << ((const char*)ptr+j) - (const char*)first_addr
                      << " memsize: " << memsize << std::endl;
            return false;
         }
      }
   }

   //Deallocate all
   for(std::size_t j = buffers.size()
      ;j--
      ;){
      sm.deallocate(buffers[j]);
   }
   if(!sm.all_memory_deallocated() && sm.check_sanity())
      return false;

   return true;
}


//This test uses tests grow and shrink_to_fit functions
template<class SegMngr>
bool test_grow_shrink_to_fit(SegMngr &sm)
{
   std::vector<void*> buffers;

   typename SegMngr::size_type original_size = sm.get_size();
   typename SegMngr::size_type original_free = sm.get_free_memory();

   sm.shrink_to_fit();

   if(!sm.all_memory_deallocated() && sm.check_sanity())
      return false;

   typename SegMngr::size_type shrunk_size          = sm.get_size();
   typename SegMngr::size_type shrunk_free_memory   = sm.get_free_memory();
   if(shrunk_size != sm.get_min_size())
      return 1;

   sm.grow(original_size - shrunk_size);

   if(!sm.all_memory_deallocated() && sm.check_sanity())
      return false;

   if(original_size != sm.get_size())
      return false;
   if(original_free != sm.get_free_memory())
      return false;

   //Allocate memory
   for(std::size_t i = 0; true; ++i){
      void *ptr = sm.allocate(i, std::nothrow);
      if(!ptr)
         break;
      std::size_t size = sm.size(ptr);
      std::memset(ptr, 0, size);
      buffers.push_back(ptr);
   }

   //Now deallocate the half of the blocks
   //so expand maybe can merge new free blocks
   for(std::size_t i = 0, max = buffers.size()
      ;i < max
      ;++i){
      if(i%2){
         sm.deallocate(buffers[i]);
         buffers[i] = 0;
      }
   }

   //Deallocate the rest of the blocks

   //Deallocate it in non sequential order
   for(std::size_t j = 0, max = buffers.size()
      ;j < max
      ;++j){
      std::size_t pos = (j%5)*(buffers.size())/4;
      if(pos == buffers.size())
         --pos;
      sm.deallocate(buffers[pos]);
      buffers.erase(buffers.begin()+std::ptrdiff_t(pos));
      typename SegMngr::size_type old_free = sm.get_free_memory();
      sm.shrink_to_fit();
      if(!sm.check_sanity())   return false;
      if(original_size < sm.get_size())    return false;
      if(old_free < sm.get_free_memory())  return false;

      sm.grow(original_size - sm.get_size());

      if(!sm.check_sanity())   return false;
      if(original_size != sm.get_size())         return false;
      if(old_free      != sm.get_free_memory())  return false;
   }

   //Now shrink it to the maximum
   sm.shrink_to_fit();

   if(sm.get_size() != sm.get_min_size())
      return 1;

   if(shrunk_free_memory != sm.get_free_memory())
      return 1;

   if(!sm.all_memory_deallocated() && sm.check_sanity())
      return false;

   sm.grow(original_size - shrunk_size);

   if(original_size != sm.get_size())
      return false;
   if(original_free != sm.get_free_memory())
      return false;

   if(!sm.all_memory_deallocated() && sm.check_sanity())
      return false;
   return true;
}

//This test allocates multiple values until there is no more memory
//and after that deallocates all in the inverse order
template<class SegMngr>
bool test_many_equal_allocation(SegMngr &sm)
{
   for( deallocation_type t = DirectDeallocation
      ; t != EndDeallocationType
      ; t = (deallocation_type)((int)t + 1)){
      typename SegMngr::size_type free_memory = sm.get_free_memory();

      std::vector<void*> buffers2;

      //Allocate buffers with extra memory
      for(std::size_t i = 0; true; ++i){
         void *ptr = sm.allocate(i, std::nothrow);
         if(!ptr)
            break;
       std::size_t size = sm.size(ptr);
         std::memset(ptr, 0, size);
         if(!sm.check_sanity())
            return false;
         buffers2.push_back(ptr);
      }

      //Now deallocate the half of the blocks
      //so expand maybe can merge new free blocks
      for(std::size_t i = 0, max = buffers2.size()
         ;i < max
         ;++i){
         if(i%2){
            sm.deallocate(buffers2[i]);
            buffers2[i] = 0;
         }
      }

      if(!sm.check_sanity())
         return false;

      typedef typename SegMngr::multiallocation_chain multiallocation_chain;
      for(std::size_t al = 1; al <= SegMngr::MemAlignment*32u; al *= 2u) {
         std::vector<void*> buffers;
         for(std::size_t i = 0; true; ++i){
            multiallocation_chain chain;
            sm.allocate_many(std::nothrow, i+1, (i+1)*2, al, chain);
            if(chain.empty())
               break;

            typename multiallocation_chain::size_type n = chain.size();
            while(!chain.empty()){
               void *ptr = ipcdetail::to_raw_pointer(chain.pop_front());
               if(!is_aligned(ptr, al))
                  return false;
               buffers.push_back(ptr);
            }
            if(n != std::size_t((i+1)*2))
               return false;
         }

         if(!sm.check_sanity())
            return false;

         switch(t){
            case DirectDeallocation:
            {
               for(std::size_t j = 0, max = buffers.size()
                  ;j < max
                  ;++j){
                  sm.deallocate(buffers[j]);
               }
            }
            break;
            case InverseDeallocation:
            {
               for(std::size_t j = buffers.size()
                  ;j--
                  ;){
                  sm.deallocate(buffers[j]);
               }
            }
            break;
            case MixedDeallocation:
            {
               for(std::size_t j = 0, max = buffers.size()
                  ;j < max
                  ;++j){
                  std::size_t pos = (j%4)*(buffers.size())/4;
                  sm.deallocate(buffers[pos]);
                  buffers.erase(buffers.begin()+std::ptrdiff_t(pos));
               }
            }
            break;
            default:
            break;
         }

         //Deallocate the rest of the blocks

         //Deallocate it in non sequential order
         for(std::size_t j = 0, max = buffers2.size()
            ;j < max
            ;++j){
            std::size_t pos = (j%4)*(buffers2.size())/4;
            sm.deallocate(buffers2[pos]);
            buffers2.erase(buffers2.begin()+std::ptrdiff_t(pos));
         }

         bool ok = free_memory == sm.get_free_memory() &&
                  sm.all_memory_deallocated() && sm.check_sanity();
         if(!ok)  return ok;
      }
   }
   return true;
}

//This test allocates multiple values until there is no more memory
//and after that deallocates all in the inverse order
template<class SegMngr>
bool test_many_different_allocation(SegMngr &sm)
{
   typedef typename SegMngr::multiallocation_chain multiallocation_chain;
   const std::size_t ArraySize = 22;
   typename SegMngr::size_type requested_sizes[ArraySize];
   for(std::size_t i = 0; i < ArraySize; ++i){
      requested_sizes[i] = 4*i;
   }

   for(std::size_t al = 1; al <= SegMngr::MemAlignment*32u; al *= 2u)
   for( deallocation_type t = DirectDeallocation
      ; t != EndDeallocationType
      ; t = (deallocation_type)((int)t + 1)){
      typename SegMngr::size_type free_memory = sm.get_free_memory();

      std::vector<void*> buffers2;

      //Allocate buffers with extra memory
      for(std::size_t i = 0; true; ++i){
         void *ptr = al > SegMngr::MemAlignment
                   ? sm.allocate_aligned(i, al, std::nothrow)
                   : sm.allocate(i, std::nothrow);
         if(!ptr)
            break;
       std::size_t size = sm.size(ptr);
         std::memset(ptr, 0, size);
         buffers2.push_back(ptr);
      }

      //Now deallocate the half of the blocks
      //so expand maybe can merge new free blocks
      for(std::size_t i = 0, max = buffers2.size()
         ;i < max
         ;++i){
         if(i%2){
            sm.deallocate(buffers2[i]);
            buffers2[i] = 0;
         }
      }

      std::vector<void*> buffers;
      while(true){
         multiallocation_chain chain;
         sm.allocate_many(std::nothrow, requested_sizes, ArraySize, 1, al, chain);
         if(chain.empty())
            break;
         typename multiallocation_chain::size_type n = chain.size();
         while(!chain.empty()){
            void *ptr = ipcdetail::to_raw_pointer(chain.pop_front());
            if(!is_aligned(ptr, al))
               return false;
            buffers.push_back(ptr);
         }
         if(n != ArraySize)
            return false;
      }

      switch(t){
         case DirectDeallocation:
         {
            for(std::size_t j = 0, max = buffers.size()
               ;j < max
               ;++j){
               sm.deallocate(buffers[j]);
            }
         }
         break;
         case InverseDeallocation:
         {
            for(std::size_t j = buffers.size()
               ;j--
               ;){
               sm.deallocate(buffers[j]);
            }
         }
         break;
         case MixedDeallocation:
         {
            for(std::size_t j = 0, max = buffers.size()
               ;j < max
               ;++j){
               std::size_t pos = (j%4)*(buffers.size())/4;
               sm.deallocate(buffers[pos]);
               buffers.erase(buffers.begin()+std::ptrdiff_t(pos));
            }
         }
         break;
         default:
         break;
      }

      //Deallocate the rest of the blocks

      //Deallocate it in non sequential order
      for(std::size_t j = 0, max = buffers2.size()
         ;j < max
         ;++j){
         std::size_t pos = (j%4)*(buffers2.size())/4;
         sm.deallocate(buffers2[pos]);
         buffers2.erase(buffers2.begin()+std::ptrdiff_t(pos));
      }

      bool ok = free_memory == sm.get_free_memory() &&
               sm.all_memory_deallocated() && sm.check_sanity();
      if(!ok)  return ok;
   }
   return true;
}

//This test allocates multiple values until there is no more memory
//and after that deallocates all in the inverse order
template<class SegMngr>
bool test_many_deallocation(SegMngr &sm)
{
   typedef typename SegMngr::multiallocation_chain multiallocation_chain;
   const std::size_t ArraySize = 11;
   boost::container::vector<multiallocation_chain> buffers;
   typename SegMngr::size_type requested_sizes[ArraySize];
   for(std::size_t i = 0; i < ArraySize; ++i){
      requested_sizes[i] = 4*i;
   }
   typename SegMngr::size_type free_memory = sm.get_free_memory();

   for(std::size_t al = 1; al <= SegMngr::MemAlignment*32u; al *= 2u)
   {
      while(true){
         multiallocation_chain chain;
         sm.allocate_many(std::nothrow, requested_sizes, ArraySize, 1, al, chain);
         if(chain.empty())
            break;
         buffers.push_back(boost::move(chain));
      }
      for(std::size_t i = 0, max = buffers.size(); i != max; ++i){
         sm.deallocate_many(buffers[i]);
      }
      buffers.clear();
      bool ok = free_memory == sm.get_free_memory() &&
               sm.all_memory_deallocated() && sm.check_sanity();
      if(!ok)  return ok;
   }

   for(std::size_t al = 1; al <= SegMngr::MemAlignment*32u; al *= 2u)
   {
      for(std::size_t i = 0; true; ++i){
         multiallocation_chain chain;
         sm.allocate_many(std::nothrow, i*4, ArraySize, al, chain);
         if(chain.empty())
            break;
         buffers.push_back(boost::move(chain));
      }
      for(std::size_t i = 0, max = buffers.size(); i != max; ++i){
         sm.deallocate_many(buffers[i]);
      }
      buffers.clear();

      bool ok = free_memory == sm.get_free_memory() &&
               sm.all_memory_deallocated() && sm.check_sanity();
      if(!ok)  return ok;
   }

   return true;
}


//This function calls all tests
template<class SegMngr>
bool test_all_allocation(SegMngr &sm)
{
   std::cout << "Starting test_allocation. Class: "
             << typeid(sm).name() << std::endl;

   if(!test_allocation(sm)){
      std::cout << "test_allocation_direct_deallocation failed. Class: "
                << typeid(sm).name() << std::endl;
      return false;
   }

   std::cout << "Starting test_many_equal_allocation. Class: "
             << typeid(sm).name() << std::endl;

   if(!test_many_equal_allocation(sm)){
      std::cout << "test_many_equal_allocation failed. Class: "
                << typeid(sm).name() << std::endl;
      return false;
   }

   std::cout << "Starting test_many_different_allocation. Class: "
             << typeid(sm).name() << std::endl;

   if(!test_many_different_allocation(sm)){
      std::cout << "test_many_different_allocation failed. Class: "
                << typeid(sm).name() << std::endl;
      return false;
   }

   if(!test_many_deallocation(sm)){
      std::cout << "test_many_deallocation failed. Class: "
                << typeid(sm).name() << std::endl;
      return false;
   }

   std::cout << "Starting test_allocation_shrink. Class: "
             << typeid(sm).name() << std::endl;

   if(!test_allocation_shrink(sm)){
      std::cout << "test_allocation_shrink failed. Class: "
                << typeid(sm).name() << std::endl;
      return false;
   }

   if(!test_allocation_shrink_and_expand(sm)){
      std::cout << "test_allocation_shrink_and_expand failed. Class: "
                << typeid(sm).name() << std::endl;
      return false;
   }

   std::cout << "Starting test_allocation_expand. Class: "
             << typeid(sm).name() << std::endl;

   if(!test_allocation_expand(sm)){
      std::cout << "test_allocation_expand failed. Class: "
                << typeid(sm).name() << std::endl;
      return false;
   }

   std::cout << "Starting test_allocation_deallocation_expand. Class: "
             << typeid(sm).name() << std::endl;

   if(!test_allocation_deallocation_expand(sm)){
      std::cout << "test_allocation_deallocation_expand failed. Class: "
                << typeid(sm).name() << std::endl;
      return false;
   }

   std::cout << "Starting test_aligned_allocation. Class: "
             << typeid(sm).name() << std::endl;

   if(!test_aligned_allocation(sm)){
      std::cout << "test_aligned_allocation failed. Class: "
                << typeid(sm).name() << std::endl;
      return false;
   }

   std::cout << "Starting test_continuous_aligned_allocation. Class: "
             << typeid(sm).name() << std::endl;

   if(!test_continuous_aligned_allocation(sm)){
      std::cout << "test_continuous_aligned_allocation failed. Class: "
                << typeid(sm).name() << std::endl;
      return false;
   }

   std::cout << "Starting test_allocation_with_reuse. Class: "
             << typeid(sm).name() << std::endl;

   if(!test_allocation_with_reuse(sm)){
      std::cout << "test_allocation_with_reuse failed. Class: "
                << typeid(sm).name() << std::endl;
      return false;
   }

   std::cout << "Starting test_clear_free_memory. Class: "
             << typeid(sm).name() << std::endl;

   if(!test_clear_free_memory(sm)){
      std::cout << "test_clear_free_memory failed. Class: "
                << typeid(sm).name() << std::endl;
      return false;
   }

   std::cout << "Starting test_grow_shrink_to_fit. Class: "
             << typeid(sm).name() << std::endl;

   if(!test_grow_shrink_to_fit(sm)){
      std::cout << "test_grow_shrink_to_fit failed. Class: "
                << typeid(sm).name() << std::endl;
      return false;
   }

   return true;
}

}}}   //namespace boost { namespace interprocess { namespace test {

#include <boost/interprocess/detail/config_end.hpp>

#endif   //BOOST_INTERPROCESS_TEST_MEMORY_ALGORITHM_TEST_TEMPLATE_HEADER


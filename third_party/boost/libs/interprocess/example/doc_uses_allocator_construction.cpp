//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2025. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/interprocess for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/interprocess/detail/workaround.hpp>
//[doc_uses_allocator_construction
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/container/vector.hpp>
#include <boost/container/uses_allocator_fwd.hpp>  //for allocator_arg_t
//<-
#include "../test/get_process_id_name.hpp"
//->

using namespace boost::interprocess;

typedef managed_shared_memory::segment_manager  seg_mngr_t;
typedef allocator<void, seg_mngr_t>             valloc_t;

struct Type    //Requires explicit allocator arguments
{
   typedef allocator<Type, seg_mngr_t> vec_alloc_t;
   boost::container::vector <Type, vec_alloc_t> m_vec;   //Recursive vector
   float m_v1;
   int   m_v2;

   Type(valloc_t va) //0-arg + alloc constructor
      : m_vec(va), m_v1(), m_v2() {}

   Type(std::size_t size, valloc_t va) //1-arg + alloc constructor
      : m_vec(vec_alloc_t(va)) //We can't use vector(size_type, allocator_type) as Type
      , m_v1(), m_v2()         //has no default constructor. Forced to one-by-one initialization.
   {  for(std::size_t i = 0; i != size; ++i)    m_vec.emplace_back(va);  }

   Type (valloc_t va, float v1, int v2) //allocator + 2-arg constructor
      : m_vec(vec_alloc_t(va)) //We can't use vector(size_type, allocator_type) as Type
      , m_v1(v1), m_v2(v2)     //has no default constructor. Forced to one-by-one initialization.
   {  for(std::size_t i = 0; i != 2; ++i) m_vec.emplace_back(va);  }
};

struct UAType   //Uses-Allocator compatible type
{
   typedef allocator<UAType, seg_mngr_t> vec_alloc_t;
   boost::container::vector <UAType, vec_alloc_t> m_vec;   //Recursive vector
   float m_v1;
   int   m_v2;

   typedef valloc_t allocator_type; //Signals uses-allocator construction is available

   UAType(valloc_t va)  //0 explicit args + allocator
      : m_vec(vec_alloc_t(va)), m_v1(), m_v2() {}
   
   UAType( boost::container::allocator_arg_t
         , allocator_type a , std::size_t size) //1 explicit arg + leading-allocator convention
      : m_vec(size, vec_alloc_t(a)), m_v1(), m_v2() {}

   UAType(float v1, int v2, allocator_type a) //2 explicit args + trailing-allocator convention
      : m_vec(vec_alloc_t(a)), m_v1(v1), m_v2(v2) {}
};

int main ()
{
   //<-
   //Remove shared memory on construction and destruction
   struct shm_remove
   {
      shm_remove() { shared_memory_object::remove(test::get_process_id_name()); }
      ~shm_remove(){ shared_memory_object::remove(test::get_process_id_name()); }
   } remover;

   (void)remover;
   //->
   managed_shared_memory ms(create_only, test::get_process_id_name(), 65536);

   // 1 arg + allocator: Requires explicit allocator argument
   //    Type(size_type, valloc_t) called
   Type *ptype1 = ms.construct< Type >(anonymous_instance)(3u, ms.get_allocator<void>());

   // allocator + 2 arg: Requires explicit allocator-convertible argument (segment_manager*)
   //    Type(valloc_t, float, int) called
   Type *ptype2 = ms.construct< Type >(anonymous_instance)(ms.get_segment_manager(), 0.0f, 0);

   // 1 explicit arg + implicit leading allocator:
   //    UAType(allocator_arg_t, allocator_type, std::size_t) called
   UAType *pua1 = ms.construct<UAType>(anonymous_instance)(3u);

   // 2 explicit args + implicit trailing allocator:
   //    UAType(float, int, allocator_type) called
   UAType *pua2 = ms.construct<UAType>(anonymous_instance)(0.0f, 0);
   //<-
   ms.destroy_ptr(ptype1);
   ms.destroy_ptr(ptype2);
   ms.destroy_ptr(pua1);
   ms.destroy_ptr(pua2);
   //->
   return 0;
}
//]

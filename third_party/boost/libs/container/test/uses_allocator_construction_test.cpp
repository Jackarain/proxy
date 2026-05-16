//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2025. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/container/uses_allocator_construction.hpp>
#include <boost/container/detail/type_traits.hpp>
#include <boost/core/lightweight_test.hpp>

typedef int    arg1_t;
typedef void * arg2_t;
typedef void (*arg3_t) (int);

typedef void *(*non_alloc_arg_t) (double*);
typedef int alloc_arg_t;

using namespace boost::container;

struct not_uses_allocator
{};

struct uses_allocator_and_not_convertible_arg
{
   bool allocator_called;
   unsigned args_called;
   typedef void (*allocator_type) (uses_allocator_and_not_convertible_arg);

   uses_allocator_and_not_convertible_arg()
      : allocator_called(false), args_called(0)
   {}

   explicit uses_allocator_and_not_convertible_arg(allocator_type)
      : allocator_called(true), args_called(0)
   {}

   explicit uses_allocator_and_not_convertible_arg(arg1_t, arg2_t, arg3_t, allocator_type)
      : allocator_called(true), args_called(3)
   {}

   explicit uses_allocator_and_not_convertible_arg(arg1_t, arg2_t, arg3_t)
      : allocator_called(false), args_called(3)
   {}

   explicit uses_allocator_and_not_convertible_arg(allocator_arg_t, allocator_type, arg1_t, arg2_t)
      : allocator_called(true), args_called(2)
   {}

   explicit uses_allocator_and_not_convertible_arg(arg1_t, arg2_t)
      : allocator_called(false), args_called(2)
   {}
};

struct uses_allocator_and_convertible_arg
{
   bool allocator_called;
   unsigned args_called;
   typedef long allocator_type;

   uses_allocator_and_convertible_arg()
      : allocator_called(false), args_called(0)
   {}

   explicit uses_allocator_and_convertible_arg(allocator_type)
      : allocator_called(true), args_called(0)
   {}

   explicit uses_allocator_and_convertible_arg(arg1_t, arg2_t, arg3_t, allocator_type)
      : allocator_called(true), args_called(3)
   {}

   explicit uses_allocator_and_convertible_arg(arg1_t, arg2_t, arg3_t)
      : allocator_called(false), args_called(3)
   {}

   explicit uses_allocator_and_convertible_arg(allocator_arg_t, allocator_type, arg1_t, arg2_t)
      : allocator_called(true), args_called(2)
   {}

   explicit uses_allocator_and_convertible_arg(arg1_t, arg2_t)
      : allocator_called(false), args_called(2)
   {}
};

struct uses_erased_type_allocator
{
   bool allocator_called;
   unsigned args_called;

   typedef erased_type allocator_type;
   typedef long constructible_from_int_t;

   uses_erased_type_allocator()
      : allocator_called(false), args_called(0)
   {}

   explicit uses_erased_type_allocator(int)
      : allocator_called(true), args_called(0)
   {}

   explicit uses_erased_type_allocator(arg1_t, arg2_t, arg3_t, constructible_from_int_t)
      : allocator_called(true), args_called(3)
   {}

   explicit uses_erased_type_allocator(arg1_t, arg2_t, arg3_t)
      : allocator_called(false), args_called(3)
   {}

   explicit uses_erased_type_allocator(allocator_arg_t, constructible_from_int_t, arg1_t, arg2_t)
      : allocator_called(true), args_called(2)
   {}

   explicit uses_erased_type_allocator(arg1_t, arg2_t)
      : allocator_called(false), args_called(2)
   {}
};

void test_uninitialized_construct()
{
   typedef dtl::aligned_storage<sizeof(uses_allocator_and_not_convertible_arg)>::type storage_t;

   //Make sure aligned_storage will be big enough
   BOOST_CONTAINER_STATIC_ASSERT( sizeof(storage_t) >= sizeof(uses_allocator_and_not_convertible_arg) );
   BOOST_CONTAINER_STATIC_ASSERT( sizeof(storage_t) >= sizeof(not_uses_allocator) );
   BOOST_CONTAINER_STATIC_ASSERT( sizeof(storage_t) >= sizeof(uses_allocator_and_convertible_arg) );
   BOOST_CONTAINER_STATIC_ASSERT( sizeof(storage_t) >= sizeof(uses_erased_type_allocator) );

   storage_t storage;
   void *const st_ptr = static_cast<void*>(storage.data);

   ////////////////////////////////////
   //not_uses_allocator
   ////////////////////////////////////
   not_uses_allocator *nua_ptr = reinterpret_cast<not_uses_allocator*>(st_ptr);
   nua_ptr = uninitialized_construct_using_allocator(nua_ptr, alloc_arg_t());

   ////////////////////////////////////
   //uses_allocator_and_convertible_arg
   ////////////////////////////////////
   uses_allocator_and_convertible_arg *uaci_ptr = reinterpret_cast<uses_allocator_and_convertible_arg*>(st_ptr);
   uaci_ptr = uninitialized_construct_using_allocator(uaci_ptr, alloc_arg_t());
   BOOST_TEST(uaci_ptr->allocator_called == true);
   BOOST_TEST(uaci_ptr->args_called == 0u);

   uaci_ptr = uninitialized_construct_using_allocator(uaci_ptr, alloc_arg_t(), arg1_t(), arg2_t(), arg3_t());
   BOOST_TEST(uaci_ptr->allocator_called == true);
   BOOST_TEST(uaci_ptr->args_called == 3u);

   uaci_ptr = uninitialized_construct_using_allocator(uaci_ptr, alloc_arg_t(), arg1_t(), arg2_t());
   BOOST_TEST(uaci_ptr->allocator_called == true);
   BOOST_TEST(uaci_ptr->args_called == 2u);

   ////////////////////////////////////
   //uses_allocator_and_not_convertible_arg
   ////////////////////////////////////
   uses_allocator_and_not_convertible_arg *uanci_ptr = reinterpret_cast<uses_allocator_and_not_convertible_arg*>(st_ptr);
   uanci_ptr = uninitialized_construct_using_allocator(uanci_ptr, alloc_arg_t());
   BOOST_TEST(uanci_ptr->allocator_called == false);
   BOOST_TEST(uanci_ptr->args_called == 0u);

   uanci_ptr = uninitialized_construct_using_allocator(uanci_ptr, alloc_arg_t(), arg1_t(), arg2_t(), arg3_t());
   BOOST_TEST(uanci_ptr->allocator_called == false);
   BOOST_TEST(uanci_ptr->args_called == 3u);

   uanci_ptr = uninitialized_construct_using_allocator(uanci_ptr, alloc_arg_t(), arg1_t(), arg2_t());
   BOOST_TEST(uanci_ptr->allocator_called == false);
   BOOST_TEST(uanci_ptr->args_called == 2u);

   uanci_ptr = uninitialized_construct_using_allocator(uanci_ptr, non_alloc_arg_t(), arg1_t(), arg2_t(), arg3_t());
   BOOST_TEST(uanci_ptr->allocator_called == false);
   BOOST_TEST(uanci_ptr->args_called == 3u);

   uanci_ptr = uninitialized_construct_using_allocator(uanci_ptr, non_alloc_arg_t(), arg1_t(), arg2_t());
   BOOST_TEST(uanci_ptr->allocator_called == false);
   BOOST_TEST(uanci_ptr->args_called == 2u);

   ////////////////////////////////////
   //uses_erased_type_allocator
   ////////////////////////////////////
   uses_erased_type_allocator *ueta_ptr = reinterpret_cast<uses_erased_type_allocator*>(st_ptr);
   ueta_ptr = uninitialized_construct_using_allocator(ueta_ptr, alloc_arg_t());
   BOOST_TEST(ueta_ptr->allocator_called == true);
   BOOST_TEST(ueta_ptr->args_called == 0u);

   ueta_ptr = uninitialized_construct_using_allocator(ueta_ptr, alloc_arg_t(), arg1_t(), arg2_t(), arg3_t());
   BOOST_TEST(ueta_ptr->allocator_called == true);
   BOOST_TEST(ueta_ptr->args_called == 3u);

   ueta_ptr = uninitialized_construct_using_allocator(ueta_ptr, alloc_arg_t(), arg1_t(), arg2_t());
   BOOST_TEST(ueta_ptr->allocator_called == true);
   BOOST_TEST(ueta_ptr->args_called == 2u);

   ueta_ptr = uninitialized_construct_using_allocator(ueta_ptr, non_alloc_arg_t(), arg1_t(), arg2_t());
   BOOST_TEST(ueta_ptr->allocator_called == false);
   BOOST_TEST(ueta_ptr->args_called == 2u);

   ueta_ptr = uninitialized_construct_using_allocator(ueta_ptr, non_alloc_arg_t(), arg1_t(), arg2_t(), arg3_t());
   BOOST_TEST(ueta_ptr->allocator_called == false);
   BOOST_TEST(ueta_ptr->args_called == 3u);
}

void test_make_obj()
{
   ////////////////////////////////////
   //not_uses_allocator
   ////////////////////////////////////
   not_uses_allocator d = make_obj_using_allocator<not_uses_allocator>(alloc_arg_t()); (void)d;

   ////////////////////////////////////
   //uses_allocator_and_convertible_arg
   ////////////////////////////////////
   uses_allocator_and_convertible_arg uaci =  make_obj_using_allocator<uses_allocator_and_convertible_arg>(alloc_arg_t());
   BOOST_TEST(uaci.allocator_called == true);
   BOOST_TEST(uaci.args_called == 0u);

   uaci =  make_obj_using_allocator<uses_allocator_and_convertible_arg>(alloc_arg_t(), arg1_t(), arg2_t(), arg3_t());
   BOOST_TEST(uaci.allocator_called == true);
   BOOST_TEST(uaci.args_called == 3u);

   uaci =  make_obj_using_allocator<uses_allocator_and_convertible_arg>(alloc_arg_t(), arg1_t(), arg2_t());
   BOOST_TEST(uaci.allocator_called == true);
   BOOST_TEST(uaci.args_called == 2u);

   ////////////////////////////////////
   //uses_allocator_and_not_convertible_arg
   ////////////////////////////////////
   uses_allocator_and_not_convertible_arg uanci = make_obj_using_allocator<uses_allocator_and_not_convertible_arg>(alloc_arg_t());
   BOOST_TEST(uanci.allocator_called == false);
   BOOST_TEST(uanci.args_called == 0u);

   uanci =  make_obj_using_allocator<uses_allocator_and_not_convertible_arg>(alloc_arg_t(), arg1_t(), arg2_t(), arg3_t());
   BOOST_TEST(uanci.allocator_called == false);
   BOOST_TEST(uanci.args_called == 3u);

   uanci =  make_obj_using_allocator<uses_allocator_and_not_convertible_arg>(alloc_arg_t(), arg1_t(), arg2_t());
   BOOST_TEST(uanci.allocator_called == false);
   BOOST_TEST(uanci.args_called == 2u);

   uanci =  make_obj_using_allocator<uses_allocator_and_not_convertible_arg>(non_alloc_arg_t(), arg1_t(), arg2_t(), arg3_t());
   BOOST_TEST(uanci.allocator_called == false);
   BOOST_TEST(uanci.args_called == 3u);

   uanci =  make_obj_using_allocator<uses_allocator_and_not_convertible_arg>(non_alloc_arg_t(), arg1_t(), arg2_t());
   BOOST_TEST(uanci.allocator_called == false);
   BOOST_TEST(uanci.args_called == 2u);

   ////////////////////////////////////
   //uses_erased_type_allocator
   ////////////////////////////////////
   uses_erased_type_allocator ueta =  make_obj_using_allocator<uses_erased_type_allocator>(alloc_arg_t());
   BOOST_TEST(ueta.allocator_called == true);
   BOOST_TEST(ueta.args_called == 0u);

   ueta =  make_obj_using_allocator<uses_erased_type_allocator>(alloc_arg_t(), arg1_t(), arg2_t(), arg3_t());
   BOOST_TEST(ueta.allocator_called == true);
   BOOST_TEST(ueta.args_called == 3u);

   ueta =  make_obj_using_allocator<uses_erased_type_allocator>(alloc_arg_t(), arg1_t(), arg2_t());
   BOOST_TEST(ueta.allocator_called == true);
   BOOST_TEST(ueta.args_called == 2u);

   ueta =  make_obj_using_allocator<uses_erased_type_allocator>(non_alloc_arg_t(), arg1_t(), arg2_t());
   BOOST_TEST(ueta.allocator_called == false);
   BOOST_TEST(ueta.args_called == 2u);

   ueta =  make_obj_using_allocator<uses_erased_type_allocator>(non_alloc_arg_t(), arg1_t(), arg2_t(), arg3_t());
   BOOST_TEST(ueta.allocator_called == false);
   BOOST_TEST(ueta.args_called == 3u);
}

int main()
{
   //test_uninitialized_construct();
   test_make_obj();
   return boost::report_errors();
}

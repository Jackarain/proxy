//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2004-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <memory>
#include <deque>
#include <iostream>
#include <list>

#include <boost/container/deque.hpp>
#include <boost/container/allocator.hpp>


#include "print_container.hpp"
#include "check_equal_containers.hpp"
#include "dummy_test_allocator.hpp"
#include "movable_int.hpp"
#include <boost/move/utility_core.hpp>
#include <boost/move/iterator.hpp>
#include <boost/container/detail/mpl.hpp>
#include <boost/container/detail/type_traits.hpp>
#include <string>
#include "emplace_test.hpp"
#include "propagate_allocator_test.hpp"
#include "vector_test.hpp"
#include "default_init_test.hpp"
#include "../../intrusive/test/iterator_test.hpp"

#include <boost/core/lightweight_test.hpp>

using namespace boost::container;

//Function to check if both sets are equal
template<class V1, class V2>
bool deque_copyable_only(V1 &, V2 &, dtl::false_type)
{
   return true;
}

//Function to check if both sets are equal
template<class V1, class V2>
bool deque_copyable_only(V1 &cntc, V2 &stdc, dtl::true_type)
{
   typedef typename V1::value_type IntType;
   std::size_t size = cntc.size();
   stdc.insert(stdc.end(), 50u, 1);
   cntc.insert(cntc.end(), 50u, IntType(1));
   if(!test::CheckEqualContainers(cntc, stdc)) return false;
   {
      IntType move_me(1);
      stdc.insert(stdc.begin()+std::ptrdiff_t(size)/2, 50u, 1);
      cntc.insert(cntc.begin()+std::ptrdiff_t(size/2), 50u, boost::move(move_me));
      if(!test::CheckEqualContainers(cntc, stdc)) return false;
   }
   {
      IntType move_me(2);
      cntc.assign(cntc.size()/2, boost::move(move_me));
      stdc.assign(stdc.size()/2, 2);
      if(!test::CheckEqualContainers(cntc, stdc)) return false;
   }
   {
      IntType move_me(1);
      stdc.clear();
      cntc.clear();
      stdc.insert(stdc.begin(), 50u, 1);
      cntc.insert(cntc.begin(), 50u, boost::move(move_me));
      if(!test::CheckEqualContainers(cntc, stdc)) return false;
      stdc.insert(stdc.begin()+20, 50u, 1);
      cntc.insert(cntc.begin()+20, 50u, boost::move(move_me));
      if(!test::CheckEqualContainers(cntc, stdc)) return false;
      stdc.insert(stdc.begin()+20, 20u, 1);
      cntc.insert(cntc.begin()+20, 20u, boost::move(move_me));
      if(!test::CheckEqualContainers(cntc, stdc)) return false;
   }
   {
      IntType move_me(1);
      stdc.clear();
      cntc.clear();
      stdc.insert(stdc.end(), 50u, 1);
      cntc.insert(cntc.end(), 50u, boost::move(move_me));
      if(!test::CheckEqualContainers(cntc, stdc)) return false;
      stdc.insert(stdc.end()-20, 50u, 1);
      cntc.insert(cntc.end()-20, 50u, boost::move(move_me));
      if(!test::CheckEqualContainers(cntc, stdc)) return false;
      stdc.insert(stdc.end()-20, 20u, 1);
      cntc.insert(cntc.end()-20, 20u, boost::move(move_me));
      if(!test::CheckEqualContainers(cntc, stdc)) return false;
   }

   return true;
}

//Test recursive structures
class recursive_deque
{
public:

   recursive_deque (const recursive_deque &x)
      : deque_(x.deque_)
   {}

   recursive_deque & operator=(const recursive_deque &x)
   {  this->deque_ = x.deque_;   return *this; }

   deque<recursive_deque> deque_;
   deque<recursive_deque>::iterator it_;
   deque<recursive_deque>::const_iterator cit_;
   deque<recursive_deque>::reverse_iterator rit_;
   deque<recursive_deque>::const_reverse_iterator crit_;
};

bool do_recursive_deque_test()
{
   //Test for recursive types
   {
      deque<recursive_deque> recursive_deque_deque;
   }

   {
      //Now test move semantics
      deque<recursive_deque> original;
      deque<recursive_deque> move_ctor(boost::move(original));
      deque<recursive_deque> move_assign;
      move_assign = boost::move(move_ctor);
      move_assign.swap(original);
   }
   return true;
}

template<class IntType, bool Reservable>
bool do_test()
{
   typedef typename deque_options<reservable<Reservable> >::type Options;
   {
      typedef deque<IntType, void, Options>  MyCnt;
      ::boost::movelib::unique_ptr<MyCnt> const pcntc = ::boost::movelib::make_unique<MyCnt>();
      pcntc->erase(pcntc->cbegin(), pcntc->cend());
   }

   //Alias deque types
   typedef deque<IntType, void, Options>  MyCnt;
   typedef std::deque<int> MyStd;
   const int max = 100;
   {
      ::boost::movelib::unique_ptr<MyCnt> const pcntc = ::boost::movelib::make_unique<MyCnt>();
      ::boost::movelib::unique_ptr<MyStd> const pstdc = ::boost::movelib::make_unique<MyStd>();
      MyCnt &cntc = *pcntc;
      MyStd &stdc = *pstdc;
      for(int i = 0; i < max*100; ++i){
         IntType move_me(i);
         cntc.insert(cntc.end(), boost::move(move_me));
         stdc.insert(stdc.end(), i);
         if(!test::CheckEqualContainers(cntc, stdc))
            return false;
      }
      if(!test::CheckEqualContainers(cntc, stdc)) return false;

      cntc.clear();
      stdc.clear();

      for(int i = 0; i < max*100; ++i){
         IntType move_me(i);
         cntc.push_back(boost::move(move_me));
         stdc.push_back(i);
         if(!test::CheckEqualContainers(cntc, stdc)) return false;
      }
      if(!test::CheckEqualContainers(cntc, stdc)) return false;

      cntc.clear();
      stdc.clear();

      for(int i = 0; i < max*100; ++i){
         IntType move_me(i);
         cntc.push_front(boost::move(move_me));
         stdc.push_front(i);
         if(!test::CheckEqualContainers(cntc, stdc))
            return false;
      }
      if(!test::CheckEqualContainers(cntc, stdc)) return false;

      typename MyCnt::iterator it;
      typename MyCnt::const_iterator cit = it;
      (void)cit;

      cntc.erase(cntc.begin()++);
      stdc.erase(stdc.begin()++);
      if(!test::CheckEqualContainers(cntc, stdc)) return false;

      cntc.erase(cntc.erase(cntc.begin()++));
      stdc.erase(stdc.erase(stdc.begin()++));
      if(!test::CheckEqualContainers(cntc, stdc)) return false;

      cntc.erase(cntc.erase(cntc.begin()+3));
      stdc.erase(stdc.erase(stdc.begin()+3));
      if(!test::CheckEqualContainers(cntc, stdc)) return false;

      cntc.erase(cntc.erase(cntc.end()-2));
      stdc.erase(stdc.erase(stdc.end()-2));
      if(!test::CheckEqualContainers(cntc, stdc)) return false;

      cntc.erase(cntc.erase(cntc.end()-4));
      stdc.erase(stdc.erase(stdc.end()-4));
      if(!test::CheckEqualContainers(cntc, stdc)) return false;

      cntc.erase(cntc.begin());
      stdc.erase(stdc.begin());
      if(!test::CheckEqualContainers(cntc, stdc)) return false;

      cntc.erase(cntc.end()-1);
      stdc.erase(stdc.end()-1);
      if(!test::CheckEqualContainers(cntc, stdc)) return false;

      {
         //Initialize values
         IntType aux_vect[50];
         for(int i = 0; i < 50; ++i){
            IntType move_me (-1);
            aux_vect[i] = boost::move(move_me);
         }
         int aux_vect2[50];
         for(int i = 0; i < 50; ++i){
            aux_vect2[i] = -1;
         }

         cntc.insert(cntc.end()
                           ,boost::make_move_iterator(&aux_vect[0])
                           ,boost::make_move_iterator(aux_vect + 50));
         stdc.insert(stdc.end(), aux_vect2, aux_vect2 + 50);
         if(!test::CheckEqualContainers(cntc, stdc)) return false;

         for(int i = 0; i < 50; ++i){
            IntType move_me (i);
            aux_vect[i] = boost::move(move_me);
         }
         for(int i = 0; i < 50; ++i){
            aux_vect2[i] = i;
         }

         cntc.insert(cntc.begin()+std::ptrdiff_t(cntc.size())
                           ,boost::make_move_iterator(&aux_vect[0])
                           ,boost::make_move_iterator(aux_vect + 50));
         stdc.insert(stdc.begin()+std::ptrdiff_t(stdc.size()), aux_vect2, aux_vect2 + 50);
         if(!test::CheckEqualContainers(cntc, stdc)) return false;

         for(int i = 0, j = static_cast<int>(cntc.size()); i < j; ++i){
            cntc.erase(cntc.begin());
            stdc.erase(stdc.begin());
         }
         if(!test::CheckEqualContainers(cntc, stdc)) return false;
      }
      {
         IntType aux_vect[50];
         for(int i = 0; i < 50; ++i){
            IntType move_me(-1);
            aux_vect[i] = boost::move(move_me);
         }
         int aux_vect2[50];
         for(int i = 0; i < 50; ++i){
            aux_vect2[i] = -1;
         }
         cntc.insert(cntc.begin()
                           ,boost::make_move_iterator(&aux_vect[0])
                           ,boost::make_move_iterator(aux_vect + 50));
         stdc.insert(stdc.begin(), aux_vect2, aux_vect2 + 50);
         if(!test::CheckEqualContainers(cntc, stdc)) return false;
      }

      if(!deque_copyable_only(cntc, stdc
                     ,dtl::bool_<boost::container::test::is_copyable<IntType>::value>())){
         return false;
      }

      cntc.erase(cntc.begin());
      stdc.erase(stdc.begin());

      if(!test::CheckEqualContainers(cntc, stdc)) return false;

      for(int i = 0; i < max; ++i){
         IntType move_me(i);
         cntc.insert(cntc.begin(), boost::move(move_me));
         stdc.insert(stdc.begin(), i);
      }
      if(!test::CheckEqualContainers(cntc, stdc)) return false;

      //Test insertion from list
      {
         std::list<int> l(50, int(1));
         cntc.insert(cntc.begin(), l.begin(), l.end());
         stdc.insert(stdc.begin(), l.begin(), l.end());
         if(!test::CheckEqualContainers(cntc, stdc)) return 1;
         cntc.assign(l.begin(), l.end());
         stdc.assign(l.begin(), l.end());
         if(!test::CheckEqualContainers(cntc, stdc)) return 1;
      }

      cntc.resize(100);
      stdc.resize(100);
      if(!test::CheckEqualContainers(cntc, stdc)) return 1;

      cntc.resize(200);
      stdc.resize(200);
      if(!test::CheckEqualContainers(cntc, stdc)) return 1;
   }

   std::cout << std::endl << "Test OK!" << std::endl;
   return true;
}

bool test_ctad()  //Older clang versions suffer from ICE here
#if !defined(BOOST_CONTAINER_NO_CXX17_CTAD) && (!defined(BOOST_CLANG) || (BOOST_CLANG_VERSION >= 80000))
{
   typedef std::vector<int> MyStd;
   //Check Constructor Template Auto Deduction
   {
      MyStd gold = MyStd{ 1, 2, 3 };
      deque<int> test = deque(gold.begin(), gold.end());
      if (!test::CheckEqualContainers(gold, test)) return false;
   }
   {
      MyStd gold = MyStd{ 1, 2, 3 };
      deque<int>  test = deque<int>(gold.begin(), gold.end(), new_allocator<int>());
      if (!test::CheckEqualContainers(gold, test)) return false;
   }
   return true;
}
#else
{ return true;  }
#endif

template<class VoidAllocator, bool Reservable>
struct GetAllocatorCont
{
   template<class ValueType>
   struct apply
   {
      typedef deque< ValueType
                    , typename allocator_traits<VoidAllocator>
                        ::template portable_rebind_alloc<ValueType>::type
                    , typename deque_options<reservable<Reservable> >::type
                    > type;
   };
};

template<class VoidAllocator, bool Reservable>
int test_cont_variants()
{
   typedef typename GetAllocatorCont<VoidAllocator, Reservable>::template apply<int>::type MyCont;
   typedef typename GetAllocatorCont<VoidAllocator, Reservable>::template apply<test::movable_int>::type MyMoveCont;
   typedef typename GetAllocatorCont<VoidAllocator, Reservable>::template apply<test::movable_and_copyable_int>::type MyCopyMoveCont;
   typedef typename GetAllocatorCont<VoidAllocator, Reservable>::template apply<test::copyable_int>::type MyCopyCont;
   typedef typename GetAllocatorCont<VoidAllocator, Reservable>::template apply<test::moveconstruct_int>::type MyMoveConstructCont;

   if (test::vector_test<MyCont>())
      return 1;
   if (test::vector_test<MyMoveCont>())
      return 1;
   if (test::vector_test<MyCopyMoveCont>())
      return 1;
   if (test::vector_test<MyCopyCont>())
      return 1;
   if (test::vector_test<MyMoveConstructCont>())
      return 1;
   return 0;
}

template<std::size_t N>
struct char_holder
{
   char chars[N];
};

bool do_test_default_block_size()
{
   //Check power of two sizes by default
   BOOST_TEST(deque<char_holder<8>  >::get_block_size() == 16*sizeof(void*));
   BOOST_TEST(deque<char_holder<12> >::get_block_size() == 16*sizeof(void*));
   BOOST_TEST(deque<char_holder<16> >::get_block_size() == 8*sizeof(void*));
   BOOST_TEST(deque<char_holder<20> >::get_block_size() == 8*sizeof(void*));
   BOOST_TEST(deque<char_holder<24> >::get_block_size() == 8*sizeof(void*));
   BOOST_TEST(deque<char_holder<28> >::get_block_size() == 8*sizeof(void*));
   BOOST_TEST(deque<char_holder<32> >::get_block_size() == 4*sizeof(void*));
   BOOST_TEST(deque<char_holder<36> >::get_block_size() == 4*sizeof(void*));
   BOOST_TEST(deque<char_holder<40> >::get_block_size() == 4*sizeof(void*));
   BOOST_TEST(deque<char_holder<44> >::get_block_size() == 4*sizeof(void*));
   BOOST_TEST(deque<char_holder<48> >::get_block_size() == 4*sizeof(void*));
   BOOST_TEST(deque<char_holder<52> >::get_block_size() == 4*sizeof(void*));
   BOOST_TEST(deque<char_holder<56> >::get_block_size() == 4*sizeof(void*));
   BOOST_TEST(deque<char_holder<60> >::get_block_size() == 4*sizeof(void*));
   BOOST_TEST(deque<char_holder<64> >::get_block_size() == 2*sizeof(void*));
   BOOST_TEST(deque<char_holder<68> >::get_block_size() == 2*sizeof(void*));
   BOOST_TEST(deque<char_holder<72> >::get_block_size() == 2*sizeof(void*));
   BOOST_TEST(deque<char_holder<76> >::get_block_size() == 2*sizeof(void*));
   BOOST_TEST(deque<char_holder<80> >::get_block_size() == 2*sizeof(void*));
   BOOST_TEST(deque<char_holder<84> >::get_block_size() == 2*sizeof(void*));
   BOOST_TEST(deque<char_holder<88> >::get_block_size() == 2*sizeof(void*));
   BOOST_TEST(deque<char_holder<92> >::get_block_size() == 2*sizeof(void*));
   //Minimal 8 elements
   BOOST_TEST(deque<char_holder<148> >::get_block_size() == 8u);
   return 0 == boost::report_errors();
}

struct boost_container_deque;

namespace boost { namespace container {   namespace test {

template<>
struct alloc_propagate_base<boost_container_deque>
{
   template <class T, class Allocator>
   struct apply
   {
      typedef boost::container::deque<T, Allocator> type;
   };
};

}}}   //namespace boost::container::test

//Test the expected sizeof()
BOOST_CONTAINER_STATIC_ASSERT_MSG(4*sizeof(void*) == sizeof(deque<int>), "sizeof has an unexpected value");

int main ()
{
   if(!do_recursive_deque_test())
      return 1;

   //Non-reservable deque
   if(!do_test<int, false>())
      return 1;

   if(!do_test<test::movable_int, true>())
      return 1;

   if(!do_test<test::movable_and_copyable_int, false>())
      return 1;

   if(!do_test<test::copyable_int, true>())
      return 1;

   if (!test_ctad())
      return 1;

   //Default block size
   if(!do_test_default_block_size())
      return 1;

   //Test non-copy-move operations
   {
      deque<test::non_copymovable_int> d;
      d.emplace_back();
      d.emplace_front(1);
      d.resize(10);
      d.resize(1);
   }


   ////////////////////////////////////
   //    Allocator implementations
   ////////////////////////////////////
   //       std:allocator
   if(test_cont_variants< std::allocator<void>, false >()){
      std::cerr << "test_cont_variants< std::allocator<void> > failed" << std::endl;
      return 1;
   }

   //       boost::container::allocator
   if(test_cont_variants< allocator<void>, true >()){
      std::cerr << "test_cont_variants< allocator<void> > failed" << std::endl;
      return 1;
   }

   ////////////////////////////////////
   //    Default init test
   ////////////////////////////////////
   if(!test::default_init_test< deque<int, test::default_init_allocator<int> > >()){
      std::cerr << "Default init test failed" << std::endl;
      return 1;
   }

   ////////////////////////////////////
   //    Emplace testing
   ////////////////////////////////////
   const test::EmplaceOptions Options = (test::EmplaceOptions)(test::EMPLACE_BACK | test::EMPLACE_FRONT | test::EMPLACE_BEFORE);

   if(!boost::container::test::test_emplace
      < deque<test::EmplaceInt>, Options>())
      return 1;
   ////////////////////////////////////
   //    Allocator propagation testing
   ////////////////////////////////////
   if(!boost::container::test::test_propagate_allocator<boost_container_deque>())
      return 1;

   ////////////////////////////////////
   //    Initializer lists testing
   ////////////////////////////////////
   if(!boost::container::test::test_vector_methods_with_initializer_list_as_argument_for
      < boost::container::deque<int> >()) {
      return 1;
   }

   ////////////////////////////////////
   //    Iterator testing
   ////////////////////////////////////
   {
      typedef boost::container::deque<int> cont_int;
      for(std::size_t i = 1; i <= 10000; i*=10){
         cont_int a;
         for (int j = 0; j < (int)i; ++j)
            a.push_back((int)j);
         boost::intrusive::test::test_iterator_random< cont_int >(a);
         if(boost::report_errors() != 0) {
            return 1;
         }
      }
   }

   ////////////////////////////////////
   //    has_trivial_destructor_after_move testing
   ////////////////////////////////////
   // default allocator
   {
      typedef boost::container::deque<int> cont;
      typedef cont::allocator_type allocator_type;
      typedef boost::container::allocator_traits<allocator_type>::pointer pointer;
      BOOST_CONTAINER_STATIC_ASSERT_MSG(!(boost::has_trivial_destructor_after_move<cont>::value !=
                                boost::has_trivial_destructor_after_move<allocator_type>::value &&
                                boost::has_trivial_destructor_after_move<pointer>::value)
                             , "has_trivial_destructor_after_move(std::allocator) test failed");
   }
   // std::allocator
   {
      typedef boost::container::deque<int, std::allocator<int> > cont;
      typedef cont::allocator_type allocator_type;
      typedef boost::container::allocator_traits<allocator_type>::pointer pointer;
      BOOST_CONTAINER_STATIC_ASSERT_MSG(!(boost::has_trivial_destructor_after_move<cont>::value !=
                               boost::has_trivial_destructor_after_move<allocator_type>::value &&
                               boost::has_trivial_destructor_after_move<pointer>::value)
                             , "has_trivial_destructor_after_move(std::allocator) test failed");
   }

   return 0;
}

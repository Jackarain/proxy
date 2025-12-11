//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2007-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <vector>
#include <deque>
#include <boost/container/vector.hpp>
#include <boost/container/deque.hpp>
#include <boost/container/devector.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/container/stable_vector.hpp>
#include <iomanip>

#include <memory>    //std::allocator
#include <iostream>  //std::cout, std::endl
#include <cstring>   //std::strcmp
#include <boost/move/detail/nsec_clock.hpp>
#include <typeinfo>

#if defined(BOOST_GCC) && (BOOST_GCC >= 40600)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
#endif

//capacity
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME capacity
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_BEG namespace boost { namespace container { namespace test {
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_END   }}}
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MIN 0
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MAX 0
#include <boost/intrusive/detail/has_member_function_callable_with.hpp>


//reserve
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME reserve
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_BEG namespace boost { namespace container { namespace test {
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_END   }}}
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MIN 1
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MAX 1
#include <boost/intrusive/detail/has_member_function_callable_with.hpp>

//back_reserve
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME reserve_back
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_BEG namespace boost { namespace container { namespace test {
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_END   }}}
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MIN 1
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MAX 1
#include <boost/intrusive/detail/has_member_function_callable_with.hpp>

//#pragma GCC diagnostic ignored "-Wunused-result"
#if defined(BOOST_GCC) && (BOOST_GCC >= 40600)
#pragma GCC diagnostic pop
#endif

using boost::move_detail::cpu_timer;
using boost::move_detail::cpu_times;
using boost::move_detail::nanosecond_type;

namespace bc = boost::container;

class MyInt
{
   int int_;

   public:
   inline explicit MyInt(int i = 0)
      : int_(i)
   {}

   inline MyInt(const MyInt &other)
      :  int_(other.int_)
   {}

   inline MyInt & operator=(const MyInt &other)
   {
      int_ = other.int_;
      return *this;
   }

   inline ~MyInt()
   {
      int_ = 0;
   }
};

class MyFatInt
{
   int int0_;
   int int1_;
   int int2_;
   int int3_;
   int int4_;
   int int5_;
   int int6_;
   int int7_;

   public:
   inline explicit MyFatInt(int i = 0)
      : int0_(i++)
      , int1_(i++)
      , int2_(i++)
      , int3_(i++)
      , int4_(i++)
      , int5_(i++)
      , int6_(i++)
      , int7_(i++)
   {}

   inline MyFatInt(const MyFatInt &other)
      : int0_(other.int0_)
      , int1_(other.int1_)
      , int2_(other.int2_)
      , int3_(other.int3_)
      , int4_(other.int4_)
      , int5_(other.int5_)
      , int6_(other.int6_)
      , int7_(other.int7_)
   {}

   inline MyFatInt & operator=(const MyFatInt &other)
   {
      int0_ = other.int0_;
      int1_ = other.int1_;
      int2_ = other.int2_;
      int3_ = other.int3_;
      int4_ = other.int4_;
      int5_ = other.int5_;
      int6_ = other.int6_;
      int7_ = other.int7_;
      return *this;
   }

   inline ~MyFatInt()
   {
      int0_ = 0u;
      int1_ = 0u;
      int2_ = 0u;
      int3_ = 0u;
      int4_ = 0u;
      int5_ = 0u;
      int6_ = 0u;
      int7_ = 0u;
   }
};

template<class C, bool Capacity = false, bool BackCapacity = false>
struct capacity_wrapper_impl
{
   inline static typename C::size_type get_capacity(const C &)
   {  return 0u; }

   inline static void set_reserve(C &, typename C::size_type )
   { }
};


template<class C>
struct capacity_wrapper_impl<C, true, false>
{
   inline static typename C::size_type get_capacity(const C &c)
   {  return c.capacity(); }

   inline static void set_reserve(C &c, typename C::size_type cp)
   {  c.reserve(cp); }
};

template<class C, bool Capacity>
struct capacity_wrapper_impl<C, Capacity, true>
{
   inline static typename C::size_type get_capacity(const C &c)
   {  return c.back_capacity(); }

   inline static void set_reserve(C &c, typename C::size_type cp)
   {  c.reserve_back(cp); }
};

template<class C>
struct capacity_wrapper
   : capacity_wrapper_impl
      < C
      , bc::test::has_member_function_callable_with_reserve<C, std::size_t>::value
      , bc::test::has_member_function_callable_with_reserve_back<C, std::size_t>::value
      >
{};


const std::size_t RangeSize = 8;

template <class IntType>
struct insert_end_range
{
   static inline std::size_t capacity_multiplier()
   {  return RangeSize;  }

   template<class C>
   inline void operator()(C &c, int)
   {  c.insert(c.end(), &a[0], &a[0]+RangeSize); }

   const char *name() const
   {  return "insert_end_range(8)"; }

   IntType a[RangeSize];
};

template <class IntType>
struct insert_end_repeated
{
   static inline std::size_t capacity_multiplier()
   {  return RangeSize;  }

   template<class C>
   inline void operator()(C &c, int i)
   {  c.insert(c.end(), RangeSize, IntType(i)); }

   inline const char *name() const
   {  return "insert_end_repeated(8)"; }

   IntType a[RangeSize];
};

template <class IntType>
struct push_back
{
   static inline std::size_t capacity_multiplier()
   {  return 1;  }

   template<class C>
   inline void operator()(C &c, int i)
   {  c.push_back(IntType(i)); }

   inline const char *name() const
   {  return "push_back"; }
};

template <class IntType>
struct emplace_back
{
   static inline std::size_t capacity_multiplier()
   {  return 1;  }

   template<class C>
   inline void operator()(C &c, int i)
   {  c.emplace_back(IntType(i)); }

   inline const char *name() const
   {  return "emplace_back"; }
};

template <class IntType>
struct insert_near_end_repeated
{
   static inline std::size_t capacity_multiplier()
   {  return RangeSize;  }

   template<class C>
   inline void operator()(C &c, int i)
   {  c.insert(c.size() >= 2*RangeSize ? c.end()-2*RangeSize : c.begin(), RangeSize, IntType(i)); }

   inline const char *name() const
   {  return "insert_near_end_repeated(8)"; }
};

template <class IntType>
struct insert_near_end_range
{
   static inline std::size_t capacity_multiplier()
   {  return RangeSize;  }

   template<class C>
   inline void operator()(C &c, int)
   {
      c.insert(c.size() >= 2*RangeSize ? c.end()-2*RangeSize : c.begin(), &a[0], &a[0]+RangeSize);
   }

   inline const char *name() const
   {  return "insert_near_end_range(8)"; }

   IntType a[RangeSize];
};

template <class IntType>
struct insert_near_end
{
   static inline std::size_t capacity_multiplier()
   {  return 1;  }

   template<class C>
   inline void operator()(C &c, int i)
   {
      typedef typename C::iterator it_t;
      it_t it (c.end());
      it -= static_cast<typename C::difference_type>(c.size() >= 2)*2;
      c.insert(it, IntType(i));
   }

   inline const char *name() const
   {  return "insert_near_end"; }
};

template <class IntType>
struct emplace_near_end
{
   static inline std::size_t capacity_multiplier()
   {
      return 1;
   }

   template<class C>
   inline void operator()(C& c, int i)
   {
      typedef typename C::iterator it_t;
      it_t it(c.end());
      it -= static_cast<typename C::difference_type>(c.size() >= 2) * 2;
      c.emplace(it, IntType(i));
   }

   inline const char* name() const
   {
      return "emplace_near_end";
   }
};

template<class Container, class Operation>
void vector_test_template(std::size_t num_iterations, std::size_t num_elements, const char *cont_name, bool prereserve = true)
{
   typedef capacity_wrapper<Container> cpw_t;

   Operation op;
   const typename Container::size_type multiplier = op.capacity_multiplier();
   Container c;
   if (prereserve) {
      cpw_t::set_reserve(c, num_elements);
   }

   cpu_timer timer;

   const std::size_t max = num_elements/multiplier;
   std::size_t capacity = 0u;

   for(std::size_t r = 0; r != num_iterations; ++r){
      //Unroll the loop to avoid noise from loop code
      int i = 0;
      if (r > num_iterations/10)  //Exclude first iterations to avoid noise
         timer.resume();
      for(std::size_t e = 0; e < max/16; ++e){
         op(c, static_cast<int>(i++));
         op(c, static_cast<int>(i++));
         op(c, static_cast<int>(i++));
         op(c, static_cast<int>(i++));
         op(c, static_cast<int>(i++));
         op(c, static_cast<int>(i++));
         op(c, static_cast<int>(i++));
         op(c, static_cast<int>(i++));
         op(c, static_cast<int>(i++));
         op(c, static_cast<int>(i++));
         op(c, static_cast<int>(i++));
         op(c, static_cast<int>(i++));
         op(c, static_cast<int>(i++));
         op(c, static_cast<int>(i++));
         op(c, static_cast<int>(i++));
         op(c, static_cast<int>(i++));
      }

      if (r > num_iterations/10)
         timer.stop();
      if(r == (num_iterations-1u))
         capacity = cpw_t::get_capacity(c);
      c.clear();
   }

   nanosecond_type nseconds = timer.elapsed().wall;

   std::cout   << cont_name << "->" << " ns: "
               << std::setw(8)
               << float(nseconds)/float((num_iterations-1)*num_elements)
               << '\t'
               << "Capacity: " << capacity
               << std::endl;
}

template<class IntType, class Operation>
void test_vectors_impl()
{
   //#define SINGLE_TEST
   #define SIMPLE_IT
   #ifdef SINGLE_TEST
      #ifdef NDEBUG
      std::size_t numit [] = { 1000 };
      #else
      std::size_t numit [] = { 20 };
      #endif
      std::size_t numele [] = { 100000 };
   #elif defined SIMPLE_IT
      #ifdef NDEBUG
      std::size_t numit [] = { 150 };
      #else
      std::size_t numit [] = { 10 };
      #endif
      std::size_t numele [] = { 500000 };
   #else
      #ifdef NDEBUG
      unsigned int numit []  = { 1000, 10000, 100000, 1000000 };
      #else
      unsigned int numit []  = { 100, 1000, 10000, 100000 };
      #endif
      unsigned int numele [] = { 10000, 1000,   100,     10       };
   #endif


#define RESERVE_ONLY 0
#define NORESERVE_ONLY 1

//#define RESERVE_STRATEGY NORESERVE_ONLY
//#define RESERVE_STRATEGY RESERVE_ONLY

#ifndef RESERVE_STRATEGY
   #define P_INIT 0
   #define P_END  2
#elif RESERVE_STRATEGY == PRERESERVE_ONLY
   #define P_INIT 1
   #define P_END  2
#elif RESERVE_STRATEGY == NORESERVE_ONLY
   #define P_INIT 0
   #define P_END  1 
#endif

   for (unsigned p = P_INIT; p != P_END; ++p) {
      std::cout << "---------------------------------\n";
      std::cout << "IntType:" << typeid(IntType).name() << " op:" << Operation().name() << ", prereserve: " << (p ? "1" : "0") << "\n";
      std::cout << "---------------------------------\n";
      const bool bp = p != 0;
      const std::size_t it_count = sizeof(numele)/sizeof(numele[0]);
      for(unsigned int i = 0; i < it_count; ++i){
         std::cout << "\n" << " ----  numit[i]: " << numit[i] << "   numele[i] : " << numele[i] << " ---- \n";
         vector_test_template< std::vector<IntType, std::allocator<IntType> >, Operation >(numit[i], numele[i],                              "std::vector    ", bp);
         vector_test_template< bc::vector<IntType, std::allocator<IntType> >, Operation >(numit[i], numele[i]        ,                       "vector         ", bp);
         vector_test_template< bc::small_vector<IntType, 0, std::allocator<IntType> >, Operation >(numit[i], numele[i],                      "small_vector   ", bp);
         vector_test_template< bc::devector<IntType, std::allocator<IntType> >, Operation >(numit[i], numele[i],                             "devector       ", bp);
         vector_test_template< std::deque<IntType, std::allocator<IntType> >, Operation >(numit[i], numele[i],                               "std::deque     ", bp);
         vector_test_template< bc::deque<IntType, std::allocator<IntType> >, Operation >(numit[i], numele[i],                                "deque          ", bp);
         vector_test_template< bc::deque<IntType, std::allocator<IntType>,
            typename bc::deque_options<bc::reservable<true> >::type       >, Operation >(numit[i], numele[i],                                "deque(reserv)  ", bp);
      }
      std::cout << "---------------------------------\n---------------------------------\n";
   }
}

template<class IntType>
void test_vectors()
{
   //end
   test_vectors_impl<IntType, push_back<IntType> >();
   #if BOOST_CXX_VERSION  >= 201103L 
   test_vectors_impl<IntType, emplace_back<IntType> >();
   #endif

   test_vectors_impl<IntType, insert_end_range<IntType> >();
   test_vectors_impl<IntType, insert_end_repeated<IntType> >();

   //near end
   test_vectors_impl<IntType, insert_near_end<IntType> >();
   #if BOOST_CXX_VERSION  >= 201103L 
   test_vectors_impl<IntType, emplace_near_end<IntType> >();
   #endif
   test_vectors_impl<IntType, insert_near_end_range<IntType> >();
   test_vectors_impl<IntType, insert_near_end_repeated<IntType> >();
}

int main()
{
   test_vectors<int>();
   test_vectors<MyInt>();
   test_vectors<MyFatInt>();

   return 0;
}

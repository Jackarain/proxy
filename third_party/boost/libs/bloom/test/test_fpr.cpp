/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#include <boost/core/lightweight_test.hpp>
#include <boost/mp11/algorithm.hpp>
#include <cmath>
#include <limits>
#include <new>
#include <string>
#include "test_types.hpp"
#include "test_utilities.hpp"

using namespace test_utilities;

template<typename T>
struct throwing_allocator
{
  using value_type=T;

  throwing_allocator()=default;
  template<typename U>
  throwing_allocator(const throwing_allocator<U>&){}

  T* allocate(std::size_t n)
  {
    return static_cast<T*>(capped_new(n*sizeof(T)));
  }

  void deallocate(T* p,std::size_t){::operator delete(p);}

  bool operator==(const throwing_allocator& x)const{return true;}
  bool operator!=(const throwing_allocator& x)const{return false;}
};

template<typename Filter>
double measure_fpr(Filter&& f,std::size_t n)
{
  using value_type=typename std::remove_reference<Filter>::type::value_type;

  value_factory<value_type> fac;
  std::size_t               res=0;
  for(std::size_t i=0;i<n;++i)f.insert(fac());
  for(std::size_t i=0;i<n;++i)res+=f.may_contain(fac());

  return (double)res/n;
}

template<typename Filter>
void test_fpr()
{
  using filter=rehash_filter<
    revalue_filter<
      realloc_filter<Filter,throwing_allocator<unsigned char>>,
      std::string
    >,
    boost::hash<std::string>
  >;

  BOOST_TEST_GT(filter(0,0.0).capacity(),0u);
  BOOST_TEST_GT(filter(0,0.5).capacity(),0u);
  BOOST_TEST_EQ(filter(0,1.0).capacity(),0u);
  BOOST_TEST_THROWS((void)filter(1,0.0),std::bad_alloc);
  BOOST_TEST_EQ(filter(100,1.0).capacity(),0u);

  {
    static constexpr int max_fpr_exp=
      std::numeric_limits<std::size_t>::digits>=64?5:3;

    for(int i=1;i<=max_fpr_exp;++i){
      std::size_t n=(std::size_t)std::pow(10.0,(double)(i+1));
      double      target_fpr=std::pow(10,(double)-i);
      double      measured_fpr=measure_fpr(filter(n,target_fpr),n);
      double      err=measured_fpr/target_fpr;
      BOOST_TEST_LE(err,2.5);
    }
  }

  BOOST_TEST_EQ(filter::fpr_for(0,1),0.0);
  BOOST_TEST_EQ(filter::fpr_for(0,0),1.0);
  BOOST_TEST_EQ(filter::fpr_for(1,0),1.0);

  {
    for(int i=1;i<=5;++i){
      double fpr1=std::pow(10.0,(double)-i);
      double fpr2=filter::fpr_for(10000,filter::capacity_for(10000,fpr1));
      BOOST_TEST_LE(std::abs((double)fpr2-fpr1)/fpr1,0.2);
    }
  }
  {
    for(int i=1;i<=5;++i){
      std::size_t m1=(std::size_t)std::pow(10.0,(double)(i+4));
      std::size_t m2=filter::capacity_for(10000,filter::fpr_for(10000,m1));
      BOOST_TEST_LE(std::abs((double)m2-m1)/m1,0.05);
    }
  }
}

struct lambda
{
  template<typename T>
  void operator()(T)
  {
    using filter=typename T::type;

    test_fpr<filter>();
  }
};

int main()
{
  boost::mp11::mp_for_each<identity_test_types>(lambda{});
  return boost::report_errors();
}

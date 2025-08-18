/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#include <boost/core/lightweight_test.hpp>
#include <boost/mp11/algorithm.hpp>
#include <climits>
#include <cmath>
#include <limits>
#include <new>
#include "test_types.hpp"
#include "test_utilities.hpp"

using namespace test_utilities;

static std::size_t num_allocations=0;

template<typename T>
struct counting_allocator
{
  using value_type=T;

  counting_allocator()=default;
  template<typename U>
  counting_allocator(const counting_allocator<U>&){}

  T* allocate(std::size_t n)
  {
    ++num_allocations;
    return static_cast<T*>(capped_new(n*sizeof(T)));
  }

  void deallocate(T* p,std::size_t){::operator delete(p);}

  bool operator==(const counting_allocator& x)const{return true;}
  bool operator!=(const counting_allocator& x)const{return false;}
};

template<typename Filter,typename ValueFactory>
void test_capacity()
{
  using filter=realloc_filter<Filter,counting_allocator<unsigned char>>;

  ValueFactory fac;

  {
    for(std::size_t n=0;n<10000;++n){
      const filter f{n};
      std::size_t  c=f.capacity();
      BOOST_TEST_EQ(c%CHAR_BIT,0);
      if(n==0)BOOST_TEST_EQ(c,0);
      else    BOOST_TEST_GE(c,n);
      BOOST_TEST_EQ(filter{c}.capacity(),c);
    }
  }
  {
    num_allocations=0;
    filter f;
    BOOST_TEST_EQ(f.capacity(),0);
    BOOST_TEST_EQ(num_allocations,0);
  }
  {
    BOOST_TEST_THROWS(
      (void)filter((std::numeric_limits<std::size_t>::max)()),
      std::bad_alloc);
  }
  {
    filter      f{{fac(),fac()},1000};
    std::size_t c=f.capacity();
    num_allocations=0;
    f.reset(f.capacity());
    BOOST_TEST_EQ(num_allocations,0);
    BOOST_TEST_EQ(f.capacity(),c);
    BOOST_TEST(f==filter{f.capacity()});
  }
  {
    filter f{{fac(),fac()},1000};
    num_allocations=0;
    f.reset();
    BOOST_TEST_EQ(num_allocations,0);
    BOOST_TEST_EQ(f.capacity(),0);
    BOOST_TEST(f==filter{});
  }
  {
    filter f{{fac(),fac()},1000};
    num_allocations=0;
    f.reset(0,1.0);
    BOOST_TEST_EQ(num_allocations,0);
    BOOST_TEST_EQ(f.capacity(),0);
    BOOST_TEST(f==filter{});
  }
  {
    filter      f{{fac(),fac()},1000};
    std::size_t c=f.capacity();
    num_allocations=0;
    f.reset(c+1);
    BOOST_TEST_EQ(num_allocations,1);
    BOOST_TEST_GE(f.capacity(),c+1);
    BOOST_TEST(f==filter{f.capacity()});
  }
  {
    filter      f;
    std::size_t c=filter::capacity_for(100,0.1);
    num_allocations=0;
    f.reset(100,0.1);
    BOOST_TEST_EQ(num_allocations,1);
    BOOST_TEST_EQ(f.capacity(),c);
  }
  {
    filter      f1{{fac(),fac()},1000},f2;
    std::size_t c=f1.capacity();
    num_allocations=0;
    f2=f1;
    BOOST_TEST_EQ(num_allocations,1);
    BOOST_TEST_GE(f2.capacity(),c);
    BOOST_TEST(f1==f2);
  }
  {
    for(int i=0;i<=5;++i){
      double fpr=std::pow(10,(double)-i);
      BOOST_TEST_EQ(
        filter::capacity_for(100,fpr),
        filter(100,fpr).capacity());
    }
  }
}

struct lambda
{
  template<typename T>
  void operator()(T)
  {
    using filter=typename T::type;
    using value_type=typename filter::value_type;

    test_capacity<filter,value_factory<value_type>>();
  }
};

int main()
{
  boost::mp11::mp_for_each<identity_test_types>(lambda{});
  return boost::report_errors();
}

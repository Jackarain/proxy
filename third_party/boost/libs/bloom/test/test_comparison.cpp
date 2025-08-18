/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#include <boost/core/lightweight_test.hpp>
#include <boost/mp11/algorithm.hpp>
#include "test_types.hpp"
#include "test_utilities.hpp"

using namespace test_utilities;

template<typename Filter,typename ValueFactory>
void test_comparison()
{
  using filter=Filter;

  ValueFactory fac;

  {
    BOOST_TEST(filter{}==filter{});
    BOOST_TEST(!(filter{}!=filter{}));
    BOOST_TEST(!(filter{}==filter{1000}));
    BOOST_TEST(filter{1000}!=filter{});
    BOOST_TEST(!(filter{1000}==filter{{fac(),fac()},1000}));
    BOOST_TEST((filter{{fac(),fac()},1000}!=filter{1000}));
  }
  {
    filter f1{1000},f2{1000};
    for(int i=0;i<10;++i){
      auto x=fac();
      f1.insert(x);
      f2.insert(x);
    }
    BOOST_TEST(f1==f2);
    BOOST_TEST(!(f2!=f1));

    for(int i=0;i<10;++i){
      auto x=fac();
      f2.insert(x);
    }
    BOOST_TEST(!(f1==f2)); /* with high prob. */
    BOOST_TEST(f2!=f1);

    const filter f3=f2;
    BOOST_TEST(f2==f3);
    BOOST_TEST(!(f3!=f2));

    f2.clear();
    BOOST_TEST(!(f2==f3));
    BOOST_TEST(f3!=f2);
  }
}

struct lambda
{
  template<typename T>
  void operator()(T)
  {
    using filter=typename T::type;
    using value_type=typename filter::value_type;

    test_comparison<filter,value_factory<value_type>>();
  }
};

int main()
{
  boost::mp11::mp_for_each<identity_test_types>(lambda{});
  return boost::report_errors();
}

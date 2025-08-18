/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#include <boost/core/lightweight_test.hpp>
#include <array>
#include <boost/mp11/algorithm.hpp>
#include <utility>
#include "test_types.hpp"
#include "test_utilities.hpp"

using namespace test_utilities;

template<typename T>
struct multiarg_constructed
{
  multiarg_constructed()=default;
  template<typename Arg1,typename Arg2,typename... Args>
  multiarg_constructed(Arg1&& arg,Arg2&&,Args&&...):
    x{std::forward<Arg1>(arg)}{}
  multiarg_constructed(const multiarg_constructed&)=delete;
  multiarg_constructed(multiarg_constructed&&)=delete;
  multiarg_constructed& operator=(const multiarg_constructed&)=default;

  operator const T&()const{return x;}

  T x;
};

template<typename Functor>
struct transparent:Functor
{
  using is_transparent=void;
  using Functor::Functor;
};

template<typename Filter,typename ValueFactory>
void test_insertion()
{
  using filter=rehash_filter<
    revalue_filter<Filter,multiarg_constructed<typename Filter::value_type>>,
    transparent<typename Filter::hasher>
  >;
  using value_type=typename filter::value_type;

  filter       f(10000);
  ValueFactory fac;

  {
    value_type x{fac(),0};
    f.insert(const_cast<value_type&>(x));
    BOOST_TEST(f.may_contain(x));
  }
  {
    value_type x{fac(),0};
    f.insert(std::move(x));
    BOOST_TEST(f.may_contain(x));
  }
  {
    auto x=fac();
    f.insert(x); /* transparent insert */
    BOOST_TEST(f.may_contain(x));
  }
  {
    std::array<value_type,10> input;
    for(auto& x:input)x={fac(),0};
    f.insert(input.begin(),input.end());
    BOOST_TEST(may_contain(f,input));
  }
  {
    std::initializer_list<value_type> il={{fac(),0},{fac(),0},{fac(),0}};
    f.insert(il);
    BOOST_TEST(may_contain(f,il));
  }
}

struct lambda
{
  template<typename T>
  void operator()(T)
  {
    using filter=typename T::type;
    using value_type=typename filter::value_type;

    test_insertion<filter,value_factory<value_type>>();
  }
};

int main()
{
  boost::mp11::mp_for_each<identity_test_types>(lambda{});
  return boost::report_errors();
}

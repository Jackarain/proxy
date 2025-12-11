/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#include <array>
#include <boost/core/lightweight_test.hpp>
#include <boost/mp11/algorithm.hpp>
#include <utility>
#include "test_types.hpp"
#include "test_utilities.hpp"

using namespace test_utilities;

template<typename Filter,typename ValueFactory>
void test_bulk_operations()
{
  using filter=Filter;
  using value_type=typename filter::value_type;

  ValueFactory fac;
  {
    filter                      f1(10000),f2(f1);
    std::array<value_type,1000> input;
    for(auto& x:input)x=fac();
    f1.insert(input.begin(),input.end());
    f2.insert(
      make_input_iterator(input.begin()),make_input_iterator(input.end()));
    BOOST_TEST(f1==f2);
  }
  {
    Filter                      f(10000);
    std::array<value_type,1000> input;
    for(auto& x:input)x=fac();
    for(std::size_t i=0;i<input.size()/2;++i)f.insert(input[i]);
    f.may_contain(input.begin(),input.end(),[&](value_type& x,bool res){
      BOOST_TEST_EQ(res,f.may_contain(x));
    });
  }
}

struct lambda
{
  template<typename T>
  void operator()(T)
  {
    using filter=typename T::type;
    using value_type=typename filter::value_type;

    test_bulk_operations<filter,value_factory<value_type>>();
  }
};

int main()
{
  boost::mp11::mp_for_each<identity_test_types>(lambda{});
  return boost::report_errors();
}

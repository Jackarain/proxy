/* For a given filter type, outputs FPR vs. c = m/n with optimum k.
 * 
 * Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#include <boost/algorithm/string/replace.hpp>
#include <boost/bloom/filter.hpp>
#include <boost/bloom/block.hpp>
#include <boost/bloom/multiblock.hpp>
#include <boost/core/detail/splitmix64.hpp>
#include <boost/mp11.hpp>
#include <boost/type_index.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <vector>

template<typename Filter>
double fpr(std::size_t c)
{
  using value_type=typename Filter::value_type;

  std::size_t num_elements=(std::size_t)(1000/Filter::fpr_for(1,c));
  std::vector<value_type> data_in,data_out;
  {
    boost::detail::splitmix64             rng;
    boost::unordered_flat_set<value_type> unique;
    for(std::size_t i=0;i<num_elements;++i){
      for(;;){
        auto x=value_type(rng());
        if(unique.insert(x).second){
          data_in.push_back(x);
          break;
        }
      }
    }
    for(std::size_t i=0;i<num_elements;++i){
      for(;;){
        auto x=value_type(rng());
        if(!unique.contains(x)){
          data_out.push_back(x);
          break;
        }
      }
    }
  }

  double fpr=0.0;
  {
    std::size_t res=0;
    Filter f(c*num_elements);
    for(const auto& x:data_in)f.insert(x);
    for(const auto& x:data_out)res+=f.may_contain(x);
    fpr=(double)res/num_elements;
  }
  return fpr;
}

using namespace boost::bloom;

/* change this definition to test another filter type */
template<std::size_t K>
using filter=boost::bloom::filter<int,1,multiblock<std::uint32_t,K>,1>;

/* change this to your desired c range */
std::size_t c_min=4,
            c_max=24;

/* you may need to change this if optimum k >= k_max */
constexpr std::size_t k_max=20;

using fpr_function=std::function<double(std::size_t)>;
static std::vector<fpr_function> fprs=[]
{
  std::vector<fpr_function> fprs;
  using ks=boost::mp11::mp_iota_c<k_max,1>;
  boost::mp11::mp_for_each<ks>([&](auto K){
    fprs.emplace_back(fpr< ::filter<K> >);
  });
  return fprs;
}();

int main()
{
  std::string filter_name=
    boost::typeindex::type_id< ::filter<666> >().pretty_name();
  boost::replace_all(filter_name,"boost::bloom::","");
  boost::replace_all(filter_name,"class ","");
  boost::replace_all(filter_name,"struct ","");
  boost::replace_all(filter_name,"666","K");

  std::cout
    <<filter_name<<"\n"
    <<"c;fpr;k\n";

  std::size_t ik=0; /* k-1 */
  for(std::size_t c=c_min;c<=c_max;++c){
    double r=fprs[ik](c);
    for(;;){
      if(ik+1>=k_max){
        std::cerr<<"k_max hit, raise it and rerun\n";
        return EXIT_FAILURE;
      }
      double rn=fprs[ik+1](c);
      if(rn>=r)break;
      r=rn;
      ++ik;
    }
    std::cout<<c<<";"<<r<<";"<<(ik+1)<<"\n";
  }
}

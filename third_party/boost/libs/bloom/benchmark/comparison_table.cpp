/* Comparison table for several configurations of boost::bloom::filter.
 * 
 * Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#include <algorithm>
#include <array>
#include <chrono>
#include <numeric>

std::chrono::high_resolution_clock::time_point measure_start,measure_pause;

template<typename F>
double measure(F f)
{
  using namespace std::chrono;

  static const int              num_trials=10;
  static const milliseconds     min_time_per_trial(10);
  std::array<double,num_trials> trials;

  for(int i=0;i<num_trials;++i){
    int                               runs=0;
    high_resolution_clock::time_point t2;
    volatile decltype(f())            res; /* to avoid optimizing f() away */

    measure_start=high_resolution_clock::now();
    do{
      res=f();
      ++runs;
      t2=high_resolution_clock::now();
    }while(t2-measure_start<min_time_per_trial);
    trials[i]=duration_cast<duration<double>>(t2-measure_start).count()/runs;
  }

  std::sort(trials.begin(),trials.end());
  return std::accumulate(
    trials.begin()+2,trials.end()-2,0.0)/(trials.size()-4);
}

void pause_timing()
{
  measure_pause=std::chrono::high_resolution_clock::now();
}

void resume_timing()
{
  measure_start+=std::chrono::high_resolution_clock::now()-measure_pause;
}

#include <boost/bloom.hpp>
#include <boost/core/detail/splitmix64.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/utility.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

template<typename T>
struct unordered_flat_set_filter
{
  using value_type=T;

  unordered_flat_set_filter(std::size_t){}
  void insert(const T& x){s.insert(x);}
  bool may_contain(const T& x){return s.contains(x);}

  boost::unordered_flat_set<T> s;
};

static std::size_t num_elements;

struct test_results
{
  double fpr;                      /* % */
  double insertion_time;           /* ns per element */
  double successful_lookup_time;   /* ns per element */
  double unsuccessful_lookup_time; /* ns per element */
};

template<typename Filter>
test_results test(std::size_t c)
{
  using value_type=typename Filter::value_type;

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
    fpr=(double)res*100/num_elements;
  }

  double insertion_time=0.0;
  {
    double t=measure([&]{
      pause_timing();
      {
        Filter f(c*num_elements);
        resume_timing();
        for(const auto& x:data_in)f.insert(x);
        pause_timing();
      }
      resume_timing();
      return 0;
    });
    insertion_time=t/num_elements*1E9;
  }

  double successful_lookup_time=0.0;
  double unsuccessful_lookup_time=0.0;
  {
    Filter f(c*num_elements);
    for(const auto& x:data_in)f.insert(x);
    double t=measure([&]{
      std::size_t res=0;
      for(const auto& x:data_in)res+=f.may_contain(x);
      return res;
    });
    successful_lookup_time=t/num_elements*1E9;
    t=measure([&]{
      std::size_t res=0;
      for(const auto& x:data_out)res+=f.may_contain(x);
      return res;
    });
    unsuccessful_lookup_time=t/num_elements*1E9;
  }

  return {fpr,insertion_time,successful_lookup_time,unsuccessful_lookup_time};
}

struct print_double
{
  print_double(double x_,int precision_=2):x{x_},precision{precision_}{}

  friend std::ostream& operator<<(std::ostream& os,const print_double& pd)
  {
    const auto default_precision{std::cout.precision()};
    os<<std::fixed<<std::setprecision(pd.precision)<<pd.x;
    std::cout.unsetf(std::ios::fixed);
    os<<std::setprecision(default_precision);
    return os;
  }

  double x;
  int    precision;
};

template<typename Filters> void row(std::size_t c)
{
  std::cout<<
    "  <tr>\n"
    "    <td align=\"center\">"<<c<<"</td>\n";

  boost::mp11::mp_for_each<
    boost::mp11::mp_transform<boost::mp11::mp_identity,Filters>
  >([&](auto i){
    using filter=typename decltype(i)::type;
    auto res=test<filter>(c);
    std::cout<<
      "    <td align=\"center\">"<<filter::k*filter::subfilter::k<<"</td>\n"
      "    <td align=\"right\">"<<print_double(res.fpr,4)<<"</td>\n"
      "    <td align=\"right\">"<<print_double(res.insertion_time)<<"</td>\n"
      "    <td align=\"right\">"<<print_double(res.successful_lookup_time)<<"</td>\n"
      "    <td align=\"right\">"<<print_double(res.unsuccessful_lookup_time)<<"</td>\n";
  });

  std::cout<<
    "  </tr>\n";
}

using namespace boost::bloom;

template<std::size_t K1,std::size_t K2,std::size_t K3>
using filters1=boost::mp11::mp_list<
  filter<int,K1>,
  filter<int,1,block<std::uint64_t,K2>>,
  filter<int,1,block<std::uint64_t,K3>,1>
>;

template<std::size_t K1,std::size_t K2,std::size_t K3>
using filters2=boost::mp11::mp_list<
  filter<int,1,multiblock<std::uint64_t,K1>>,
  filter<int,1,multiblock<std::uint64_t,K2>,1>,
  filter<int,1,fast_multiblock32<K3>>
>;

template<std::size_t K1,std::size_t K2,std::size_t K3>
using filters3=boost::mp11::mp_list<
  filter<int,1,fast_multiblock32<K1>,1>,
  filter<int,1,fast_multiblock64<K2>>,
  filter<int,1,fast_multiblock64<K3>,1>
>;

template<std::size_t K1,std::size_t K2,std::size_t K3>
using filters4=boost::mp11::mp_list<
  filter<int,1,block<std::uint64_t[8],K1>>,
  filter<int,1,block<std::uint64_t[8],K2>,1>,
  filter<int,1,multiblock<std::uint64_t[8],K3>>
>;

int main(int argc,char* argv[])
{
  if(argc<2){
    std::cerr<<"provide the number of elements\n";
    return EXIT_FAILURE;
  }
  try{
    num_elements=std::stoul(argv[1]);
  }
  catch(...){
    std::cerr<<"wrong arg\n";
    return EXIT_FAILURE;
  }

  /* reference table: boost::unordered_flat_set */

  auto res=test<unordered_flat_set_filter<int>>(0);
  std::cout<<
    "<table>\n"
    "  <tr><th colspan=\"3\"><code>boost::unordered_flat_set</code></tr>\n"
    "  <tr>\n"
    "    <th>insertion</th>\n"
    "    <th>successful<br/>lookup</th>\n"
    "    <th>unsuccessful<br/>lookup</th>\n"
    "  </tr>\n"
    "  <tr>\n"
    "    <td align=\"right\">"<<print_double(res.insertion_time)<<"</td>\n"
    "    <td align=\"right\">"<<print_double(res.successful_lookup_time)<<"</td>\n"
    "    <td align=\"right\">"<<print_double(res.unsuccessful_lookup_time)<<"</td>\n"
    "  </tr>\n"
    "</table>\n";

  /* filter table */

  auto subheader=
    "    <th>K</th>\n"
    "    <th>FPR<br/>[%]</th>\n"
    "    <th>ins.</th>\n"
    "    <th>succ.<br/>lkp.</th>\n"
    "    <th>uns.<br/>lkp.</th>\n";

  std::cout<<
    "<table>\n"
    "  <tr>\n"
    "    <th></th>\n"
    "    <th colspan=\"5\"><code>filter&lt;int,K></code></th>\n"
    "    <th colspan=\"5\"><code>filter&lt;int,1,block&lt;uint64_t,K>></code></th>\n"
    "    <th colspan=\"5\"><code>filter&lt;int,1,block&lt;uint64_t,K>,1></code></th>\n"
    "  </tr>\n"
    "  <tr>\n"
    "    <th>c</th>\n"<<
    subheader<<
    subheader<<
    subheader<<
    "  </tr>\n";

  row<filters1< 6,  4,  5>>( 8);
  row<filters1< 9,  5,  6>>(12);
  row<filters1<11,  6,  7>>(16);
  row<filters1<14,  7,  8>>(20);

  std::cout<<
    "  <tr>\n"
    "    <th></th>\n"
    "    <th colspan=\"5\"><code>filter&lt;int,1,multiblock&lt;uint64_t,K>></code></th>\n"
    "    <th colspan=\"5\"><code>filter&lt;int,1,multiblock&lt;uint64_t,K>,1></code></th>\n"
    "    <th colspan=\"5\"><code>filter&lt;int,1,fast_multiblock32&lt;K>></code></th>\n"
    "  </tr>\n"
    "  <tr>\n"
    "    <th>c</th>\n"<<
    subheader<<
    subheader<<
    subheader<<
    "  </tr>\n";

  row<filters2< 5,  5,  5>>( 8);
  row<filters2< 8,  8,  8>>(12);
  row<filters2<11, 11, 11>>(16);
  row<filters2<13, 14, 13>>(20);

  std::cout<<
    "  <tr>\n"
    "    <th></th>\n"
    "    <th colspan=\"5\"><code>filter&lt;int,1,fast_multiblock32&lt;K>,1></code></th>\n"
    "    <th colspan=\"5\"><code>filter&lt;int,1,fast_multiblock64&lt;K>></code></th>\n"
    "    <th colspan=\"5\"><code>filter&lt;int,1,fast_multiblock64&lt;K>,1></code></th>\n"
    "  </tr>\n"
    "  <tr>\n"
    "    <th>c</th>\n"<<
    subheader<<
    subheader<<
    subheader<<
    "  </tr>\n";

  row<filters3< 5,  5,  5>>( 8);
  row<filters3< 8,  8,  8>>(12);
  row<filters3<11, 11, 11>>(16);
  row<filters3<13, 13, 14>>(20);

  std::cout<<
    "  <tr>\n"
    "    <th></th>\n"
    "    <th colspan=\"5\"><code>filter&lt;int,1,block&lt;uint64_t[8],K>></code></th>\n"
    "    <th colspan=\"5\"><code>filter&lt;int,1,block&lt;uint64_t[8],K>,1></code></th>\n"
    "    <th colspan=\"5\"><code>filter&lt;int,1,multiblock&lt;uint64_t[8],K>></code></th>\n"
    "  </tr>\n"
    "  <tr>\n"
    "    <th>c</th>\n"<<
    subheader<<
    subheader<<
    subheader<<
    "  </tr>\n";

  row<filters4< 5,  6,  7>>( 8);
  row<filters4< 7,  7, 10>>(12);
  row<filters4< 9, 10, 11>>(16);
  row<filters4<12, 12, 15>>(20);

  std::cout<<"</table>\n";
}

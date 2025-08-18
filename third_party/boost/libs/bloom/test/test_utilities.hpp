/* Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#ifndef BOOST_BLOOM_TEST_TEST_UTILITIES_HPP
#define BOOST_BLOOM_TEST_TEST_UTILITIES_HPP

#include <boost/bloom/filter.hpp>
#include <limits>
#include <new>
#include <string>

namespace test_utilities{

template<typename T>
struct value_factory
{
  T operator()(){return n++;}
  T n=0;
};

template<>
struct value_factory<std::string>
{
  std::string operator()()
  {
    return std::to_string(n++);
  }

  int n=0;
};

template<typename Filter,typename U>
struct revalue_filter_impl;

template<
  typename T,std::size_t K,typename S,std::size_t B,typename H,typename A,
  typename U
>
struct revalue_filter_impl<boost::bloom::filter<T,K,S,B,H,A>,U>
{
  using type=boost::bloom::filter<U,K,S,B,H,A>;
};

template<typename Filter,typename U>
using revalue_filter=typename revalue_filter_impl<Filter,U>::type;

template<typename Filter,typename Hash>
struct rehash_filter_impl;

template<
  typename T,std::size_t K,typename S,std::size_t B,typename H,typename A,
  typename Hash
>
struct rehash_filter_impl<boost::bloom::filter<T,K,S,B,H,A>,Hash>
{
  using type=boost::bloom::filter<T,K,S,B,Hash,A>;
};

template<typename Filter,typename Hash>
using rehash_filter=typename rehash_filter_impl<Filter,Hash>::type;

template<typename Filter,typename Allocator>
struct realloc_filter_impl;

template<
  typename T,std::size_t K,typename S,std::size_t B,typename H,typename A,
  typename Allocator
>
struct realloc_filter_impl<boost::bloom::filter<T,K,S,B,H,A>,Allocator>
{
  using type=boost::bloom::filter<T,K,S,B,H,Allocator>;
};

template<typename Filter,typename Allocator>
using realloc_filter=typename realloc_filter_impl<Filter,Allocator>::type;

void* capped_new(std::size_t n)
{
  using limits=std::numeric_limits<std::size_t>;
  static constexpr std::size_t alloc_limit=
    limits::digits>=64?
    /* avoid AddressSanitizer: allocation-size-too-big */
    (std::size_t)0x10000000000ull:
    /* avoid big allocations that might succeed in 32-bit */
    (limits::max)()/256;

  if(n>alloc_limit)throw std::bad_alloc{};

  return ::operator new(n);
}

template<typename Filter,typename Input>
bool may_contain(const Filter& f,const Input& input)
{
  std::size_t res=0;
  for(const auto& x:input)res+=f.may_contain(x);
  return res==input.size();
}

template<typename Filter,typename Input>
bool may_not_contain(const Filter& f,const Input& input)
{
  std::size_t res=0;
  for(const auto& x:input)res+=f.may_contain(x);
  return res<input.size(); /* res should be 0 with high probability */
}

} /* namespace test_utilities */
#endif

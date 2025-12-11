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
#include <iterator>
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
std::size_t may_contain_count(const Filter& f,const Input& input)
{
  using input_value_type=typename Input::value_type;
  std::size_t res=0;
  f.may_contain(
    input.begin(),input.end(),
    [&](const input_value_type&,bool b){res+=b;});
  return res;
}

template<typename Filter,typename Input>
bool may_contain(const Filter& f,const Input& input)
{
  return may_contain_count(f,input)==input.size();
}

template<typename Filter,typename Input>
bool may_not_contain(const Filter& f,const Input& input)
{
  /* may_contain_count should be 0 with high probability */
  return may_contain_count(f,input)<input.size();
}

template<typename Iterator>
class input_iterator
{
  using traits=std::iterator_traits<Iterator>;
  Iterator it;

public:
  using iterator_category=std::input_iterator_tag;
  using value_type=typename traits::value_type;
  using difference_type=typename traits::difference_type;
  using pointer=Iterator;
  using reference=typename traits::reference;

  input_iterator(Iterator it_):it{it_}{}
  reference operator*()const{return *it;}
  pointer operator->()const{return it;}
  input_iterator& operator++(){++it;return *this;}
  input_iterator operator++(int){auto res=*this;++it;return res;}
  bool operator==(const input_iterator& x)const{return it==x.it;}
  bool operator!=(const input_iterator& x)const{return !(*this==x);}
};

template<typename Iterator>
input_iterator<Iterator> make_input_iterator(Iterator it){return {it};}

} /* namespace test_utilities */
#endif

/* Copyright 2024 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#ifndef BOOST_POLY_COLLECTION_TEST_VARIANT_TYPES_HPP
#define BOOST_POLY_COLLECTION_TEST_VARIANT_TYPES_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/poly_collection/variant_collection.hpp>
#include <string>

namespace variant_types{

struct alternative1
{
  alternative1(int n=0):n{n}{}
  int operator()(int x)const{return x;}
  bool operator==(const alternative1& x)const{return n==x.n;}
  int n;
};

struct alternative2
{
  alternative2(int n=0):n{n}{}
  int operator()(int x)const{return x;}
  bool operator==(const alternative2& x)const{return n==x.n;}
  long int n;
};

struct alternative3
{
  alternative3(int n):n{n}{}
  alternative3(alternative3&&)=default;
  alternative3& operator=(alternative3&&)=delete;
  int operator()(int x)const{return x*n;}
  bool operator==(const alternative3& x)const{return n==x.n;}
  int n;
};

struct alternative4:alternative3
{
  using alternative3::alternative3;
  bool operator==(const alternative4& x)const{return n==x.n;}
};

struct alternative5
{
  alternative5(int n):str{std::to_string(n)}{}
  alternative5(alternative5&&)=default;
  alternative5& operator=(alternative5&&)=delete;
  int operator()(int x)const{return x*std::stoi(str);}
  std::string str;
};

using collection=boost::variant_collection_of<
  alternative1,alternative2,alternative3,alternative4,alternative5
>;

using value_type=collection::value_type;

using t1=alternative1;
using t2=alternative2;
using t3=alternative3;
using t4=alternative4;
using t5=alternative5;

/* As t5 is not equality comparable, define operator== for value_type and make
 * intertype equality comparison work variant style.
 */

struct eq_
{
  template<typename T>
  bool operator ()(const T& x,const T& y)const{return x==y;}

  template<typename T,typename Q>
  bool operator()(const T& x,const Q& y)const{return false;}

  bool operator ()(const t5&,const t5&)const{return false;}
};

inline bool operator==(const value_type& x,const value_type& y)
{
  return boost::poly_collection::visit(eq_{},x,y);
}

template<typename T>
struct bounded_eq_
{
  const T& x;

  bool operator ()(const T& y)const{return x==y;}

  template<typename Q>bool operator()(const Q&)const{return false;}
};

template<typename T> using is_alternative=
  boost::mp11::mp_contains<boost::mp11::mp_list<t1,t2,t3,t4,t5>,T>;

template<
  typename T,
  typename std::enable_if<is_alternative<T>::value>::type* = nullptr
>
inline bool operator==(const value_type& x,const T& y)
{
  return boost::poly_collection::visit(bounded_eq_<T>{y},x);
}

template<
  typename T,
  typename std::enable_if<is_alternative<T>::value>::type* = nullptr
>
inline bool operator==(const T& x,const value_type& y)
{
  return y==x;
}

inline bool operator==(const t1&,const t2&){return false;}
inline bool operator==(const t1&,const t3&){return false;}
inline bool operator==(const t1&,const t4&){return false;}
inline bool operator==(const t1&,const t5&){return false;}
inline bool operator==(const t2&,const t1&){return false;}
inline bool operator==(const t2&,const t3&){return false;}
inline bool operator==(const t2&,const t4&){return false;}
inline bool operator==(const t2&,const t5&){return false;}
inline bool operator==(const t3&,const t1&){return false;}
inline bool operator==(const t3&,const t2&){return false;}
inline bool operator==(const t3&,const t4&){return false;}
inline bool operator==(const t3&,const t5&){return false;}
inline bool operator==(const t4&,const t1&){return false;}
inline bool operator==(const t4&,const t2&){return false;}
inline bool operator==(const t4&,const t3&){return false;}
inline bool operator==(const t4&,const t5&){return false;}
inline bool operator==(const t5&,const t1&){return false;}
inline bool operator==(const t5&,const t2&){return false;}
inline bool operator==(const t5&,const t3&){return false;}
inline bool operator==(const t5&,const t4&){return false;}

struct to_int
{
  int operator()(const value_type& x)const
  {
    return boost::poly_collection::visit(*this,x);
  }

  int operator()(const t1& x)const{return x.n;}
  int operator()(const t2& x)const{return (int)x.n;}
  int operator()(const t3& x)const{return x.n;}
  int operator()(const t4& x)const{return x.n;}
  int operator()(const t5& x)const{return std::stoi(x.str);}
};

} /* namespace variant_types */

#endif

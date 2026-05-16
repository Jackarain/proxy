/* Boost.MultiIndex test for composite_key.
 *
 * Copyright 2003-2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/multi_index for library home page.
 */

#include "test_composite_key.hpp"

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <boost/detail/lightweight_test.hpp>
#include "pre_multi_index.hpp"
#include <boost/mp11/utility.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>

using namespace boost::multi_index;

template<typename T>
struct composite_object_length:std::tuple_size<T>{};

template<typename CompositeKey>
struct composite_object_length<composite_key_result<CompositeKey>>:
  std::tuple_size<
    typename composite_key_result<CompositeKey>::
      composite_key_type::key_extractor_tuple
  >
{};

template<typename CompositeKeyResult,typename T2>
struct comparison_equal_length
{
  static bool is_less(const CompositeKeyResult& x,const T2& y)
  {
    composite_key_result_equal_to<CompositeKeyResult> eq;
    composite_key_result_less<CompositeKeyResult>     lt;
    composite_key_result_greater<CompositeKeyResult>  gt;
    std::equal_to<CompositeKeyResult>                 std_eq;
    std::less<CompositeKeyResult>                     std_lt;
    std::greater<CompositeKeyResult>                  std_gt;

    return  (x< y) && !(y< x)&&
           !(x==y) && !(y==x)&&
            (x!=y) &&  (y!=x)&&
           !(x> y) &&  (y> x)&&
           !(x>=y) &&  (y>=x)&&
            (x<=y) && !(y<=x)&&
          !eq(x,y) && !eq(y,x)&&
           lt(x,y) && !lt(y,x)&&
          !gt(x,y) &&  gt(y,x)&&
      !std_eq(x,y) && !std_eq(y,x)&&
       std_lt(x,y) && !std_lt(y,x)&&
      !std_gt(x,y) &&  std_gt(y,x);
  }

  static bool is_greater(const CompositeKeyResult& x,const T2& y)
  {
    composite_key_result_equal_to<CompositeKeyResult> eq;
    composite_key_result_less<CompositeKeyResult>     lt;
    composite_key_result_greater<CompositeKeyResult>  gt;
    std::equal_to<CompositeKeyResult>                 std_eq;
    std::less<CompositeKeyResult>                     std_lt;
    std::greater<CompositeKeyResult>                  std_gt;

    return !(x< y) &&  (y< x)&&
           !(x==y) && !(y==x)&&
            (x!=y) &&  (y!=x)&&
            (x> y) && !(y> x)&&
            (x>=y) && !(y>=x)&&
           !(x<=y) &&  (y<=x)&&
          !eq(x,y) && !eq(y,x)&&
          !lt(x,y) &&  lt(y,x)&&
           gt(x,y) && !gt(y,x)&&
      !std_eq(x,y) && !std_eq(y,x)&&
      !std_lt(x,y) &&  std_lt(y,x)&&
       std_gt(x,y) && !std_gt(y,x);
  }

  static bool is_equiv(const CompositeKeyResult& x,const T2& y)
  {
    composite_key_result_equal_to<CompositeKeyResult> eq;
    composite_key_result_less<CompositeKeyResult>     lt;
    composite_key_result_greater<CompositeKeyResult>  gt;
    std::equal_to<CompositeKeyResult>                 std_eq;
    std::less<CompositeKeyResult>                     std_lt;
    std::greater<CompositeKeyResult>                  std_gt;

    return !(x< y) && !(y< x)&&
            (x==y) &&  (y==x)&&
           !(x!=y) && !(y!=x)&&
           !(x> y) && !(y> x)&&
            (x>=y) &&  (y>=x)&&
            (x<=y) &&  (y<=x)&&
           eq(x,y) &&  eq(y,x)&&
          !lt(x,y) && !lt(y,x)&&
          !gt(x,y) && !gt(y,x)&&
       std_eq(x,y) &&  std_eq(y,x)&&
      !std_lt(x,y) && !std_lt(y,x)&&
      !std_gt(x,y) && !std_gt(y,x);
  }
};

template<typename CompositeKeyResult,typename T2>
struct comparison_different_length
{
  static bool is_less(const CompositeKeyResult& x,const T2& y)
  {
    composite_key_result_less<CompositeKeyResult>    lt;
    composite_key_result_greater<CompositeKeyResult> gt;
    std::less<CompositeKeyResult>                    std_lt;
    std::greater<CompositeKeyResult>                 std_gt;

    return  (x< y) && !(y< x)&&
           !(x> y) &&  (y> x)&&
           !(x>=y) &&  (y>=x)&&
            (x<=y) && !(y<=x)&&
           lt(x,y) && !lt(y,x)&&
          !gt(x,y) &&  gt(y,x)&&
       std_lt(x,y) && !std_lt(y,x)&&
      !std_gt(x,y) &&  std_gt(y,x);
  }

  static bool is_greater(const CompositeKeyResult& x,const T2& y)
  {
    composite_key_result_less<CompositeKeyResult>    lt;
    composite_key_result_greater<CompositeKeyResult> gt;
    std::less<CompositeKeyResult>                    std_lt;
    std::greater<CompositeKeyResult>                 std_gt;

    return !(x< y) &&  (y< x)&&
            (x> y) && !(y> x)&&
            (x>=y) && !(y>=x)&&
           !(x<=y) &&  (y<=x)&&
          !lt(x,y) &&  lt(y,x)&&
           gt(x,y) && !gt(y,x)&&
      !std_lt(x,y) && std_lt(y,x)&&
       std_gt(x,y) && !std_gt(y,x);
  }

  static bool is_equiv(const CompositeKeyResult& x,const T2& y)
  {
    composite_key_result_less<CompositeKeyResult>    lt;
    composite_key_result_greater<CompositeKeyResult> gt;
    std::less<CompositeKeyResult>                    std_lt;
    std::greater<CompositeKeyResult>                 std_gt;

    return !(x< y) && !(y< x)&&
           !(x> y) && !(y> x)&&
            (x>=y) &&  (y>=x)&&
            (x<=y) &&  (y<=x)&&
          !lt(x,y) && !lt(y,x)&&
          !gt(x,y) && !gt(y,x)&&
      !std_lt(x,y) && !std_lt(y,x)&&
      !std_gt(x,y) && !std_gt(y,x);
  }
};

template<typename CompositeKeyResult,typename T2>
struct comparison_helper:
  boost::mp11::mp_if_c<
    composite_object_length<CompositeKeyResult>::value==
      composite_object_length<T2>::value,
    comparison_equal_length<CompositeKeyResult,T2>,
    comparison_different_length<CompositeKeyResult,T2>
  >
{
};

template<typename CompositeKeyResult,typename T2>
static bool is_less(const CompositeKeyResult& x,const T2& y)
{
  return comparison_helper<CompositeKeyResult,T2>::is_less(x,y);
}

template<typename CompositeKeyResult,typename T2>
static bool is_greater(const CompositeKeyResult& x,const T2& y)
{
  return comparison_helper<CompositeKeyResult,T2>::is_greater(x,y);
}

template<typename CompositeKeyResult,typename T2>
static bool is_equiv(const CompositeKeyResult& x,const T2& y)
{
  return comparison_helper<CompositeKeyResult,T2>::is_equiv(x,y);
}

template<typename T1,typename T2,typename Compare>
static bool is_less(const T1& x,const T2& y,const Compare& c)
{
  return c(x,y)&&!c(y,x);
}

template<typename T1,typename T2,typename Compare>
static bool is_greater(const T1& x,const T2& y,const Compare& c)
{
  return c(y,x)&&!c(x,y);
}

template<typename T1,typename T2,typename Compare>
static bool is_equiv(const T1& x,const T2& y,const Compare& c)
{
  return !c(x,y)&&!c(y,x);
}

template<typename T1,typename T2,typename Compare,typename Equiv>
static bool is_less(
  const T1& x,const T2& y,const Compare& c,const Equiv& eq)
{
  return c(x,y)&&!c(y,x)&&!eq(x,y)&&!eq(y,x);
}

template<typename T1,typename T2,typename Compare,typename Equiv>
static bool is_greater(
  const T1& x,const T2& y,const Compare& c,const Equiv& eq)
{
  return c(y,x)&&!c(x,y)&&!eq(x,y)&&!eq(y,x);
}

template<typename T1,typename T2,typename Compare,typename Equiv>
static bool is_equiv(
  const T1& x,const T2& y,const Compare& c,const Equiv& eq)
{
  return !c(x,y)&&!c(y,x)&&eq(x,y)&&eq(y,x);
}

struct xyz
{
  xyz(int x_=0,int y_=0,int z_=0):x(x_),y(y_),z(z_){}

  int  x;
  int  y;
  int  z;
};

struct modulo_equal
{
  modulo_equal(int i):i_(i){}
  bool operator ()(int x,int y)const{return (x%i_)==(y%i_);}

private:
  int i_;
};

struct xystr
{
  xystr(int x_=0,int y_=0,std::string str_=""):x(x_),y(y_),str(str_){}

  int         x;
  int         y;
  std::string str;
};

template<template<typename...>class Tuple>
struct tuple_maker
{
  template<typename... Ts>
  static Tuple<Ts...> create(const Ts&... args){return Tuple<Ts...>{args...};}
};

using boost_tuple_maker=tuple_maker<boost::tuple>;
using std_tuple_maker=tuple_maker<std::tuple>;

template<typename TupleMaker>
void test_composite_key_template()
{
  typedef composite_key<
    xyz,
    BOOST_MULTI_INDEX_MEMBER(xyz,int,x),
    BOOST_MULTI_INDEX_MEMBER(xyz,int,y),
    BOOST_MULTI_INDEX_MEMBER(xyz,int,z)
  > ckey_t1;

  typedef multi_index_container<
    xyz,
    indexed_by<
      ordered_unique<ckey_t1>
    >
  > indexed_t1;

  indexed_t1 mc1;
  mc1.insert(xyz(0,0,0));
  mc1.insert(xyz(0,0,1));
  mc1.insert(xyz(0,1,0));
  mc1.insert(xyz(0,1,1));
  mc1.insert(xyz(1,0,0));
  mc1.insert(xyz(1,0,1));
  mc1.insert(xyz(1,1,0));
  mc1.insert(xyz(1,1,1));

  BOOST_TEST(mc1.size()==8);
  BOOST_TEST(
    std::distance(
      mc1.find(mc1.key_extractor()(xyz(0,0,0))),
      mc1.find(mc1.key_extractor()(xyz(1,0,0))))==4);
  BOOST_TEST(
    std::distance(
      mc1.find(TupleMaker::create(0,0,0)),
      mc1.find(TupleMaker::create(1,0,0)))==4);
  BOOST_TEST(
    std::distance(
      mc1.lower_bound(TupleMaker::create(0,0)),
      mc1.upper_bound(TupleMaker::create(1,0)))==6);
  BOOST_TEST(
    std::distance(
      mc1.lower_bound(1),
      mc1.upper_bound(1))==4);

  ckey_t1 ck1;
  ckey_t1 ck2(ck1);
  ckey_t1 ck3(
    boost::make_tuple(
      BOOST_MULTI_INDEX_MEMBER(xyz,int,x)(),
      BOOST_MULTI_INDEX_MEMBER(xyz,int,y)(),
      BOOST_MULTI_INDEX_MEMBER(xyz,int,z)()));
  ckey_t1 ck4(get<0>(ck1.key_extractors()));
  ckey_t1 ck5(
    std::make_tuple(
      BOOST_MULTI_INDEX_MEMBER(xyz,int,x)(),
      BOOST_MULTI_INDEX_MEMBER(xyz,int,y)(),
      BOOST_MULTI_INDEX_MEMBER(xyz,int,z)()));

  ck3=ck5; /* prevent unused var */

  get<2>(ck4.key_extractors())=
    get<2>(ck2.key_extractors());

  BOOST_TEST(is_equiv  (ck1(xyz(0,0,0)),ck2(xyz(0,0,0))));
  BOOST_TEST(is_less   (ck1(xyz(0,0,1)),ck2(xyz(0,1,0))));
  BOOST_TEST(is_greater(ck1(xyz(0,0,1)),ck2(xyz(0,0,0))));

  BOOST_TEST(is_equiv  (ck1(xyz(0,0,0)),TupleMaker::create(0)));
  BOOST_TEST(is_less   (ck1(xyz(0,0,0)),TupleMaker::create(1)));
  BOOST_TEST(is_greater(ck1(xyz(0,0,0)),TupleMaker::create(-1)));
  BOOST_TEST(is_equiv  (ck1(xyz(0,0,0)),TupleMaker::create(0,0)));
  BOOST_TEST(is_less   (ck1(xyz(0,0,0)),TupleMaker::create(0,1)));
  BOOST_TEST(is_greater(ck1(xyz(0,0,0)),TupleMaker::create(0,-1)));
  BOOST_TEST(is_equiv  (ck1(xyz(0,0,0)),TupleMaker::create(0,0,0)));
  BOOST_TEST(is_less   (ck1(xyz(0,0,0)),TupleMaker::create(0,0,1)));
  BOOST_TEST(is_greater(ck1(xyz(0,0,0)),TupleMaker::create(0,0,-1)));
  BOOST_TEST(is_equiv  (ck1(xyz(0,0,0)),TupleMaker::create(0,0,0,1)));

  typedef composite_key_result_less<ckey_t1::result_type>     ckey_comp_t1;
  typedef composite_key_result_equal_to<ckey_t1::result_type> ckey_eq_t1;

  ckey_comp_t1 cp1;
  ckey_eq_t1   eq1;

  BOOST_TEST(is_equiv  (ck1(xyz(0,0,0)),ck2(xyz(0,0,0)),cp1,eq1));
  BOOST_TEST(is_less   (ck1(xyz(0,0,1)),ck2(xyz(0,1,0)),cp1,eq1));
  BOOST_TEST(is_greater(ck1(xyz(0,0,1)),ck2(xyz(0,0,0)),cp1,eq1));

  BOOST_TEST(is_equiv  (ck1(xyz(0,0,0)),TupleMaker::create(0),cp1));
  BOOST_TEST(is_less   (ck1(xyz(0,0,0)),TupleMaker::create(1),cp1));
  BOOST_TEST(is_greater(ck1(xyz(0,0,0)),TupleMaker::create(-1),cp1));

  BOOST_TEST(is_equiv  (ck1(xyz(0,0,0)),0,cp1));
  BOOST_TEST(is_less   (ck1(xyz(0,0,0)),1,cp1));
  BOOST_TEST(is_greater(ck1(xyz(0,0,0)),-1,cp1));

  BOOST_TEST(is_equiv  (ck1(xyz(0,0,0)),TupleMaker::create(0,0),cp1));
  BOOST_TEST(is_less   (ck1(xyz(0,0,0)),TupleMaker::create(0,1),cp1));
  BOOST_TEST(is_greater(ck1(xyz(0,0,0)),TupleMaker::create(0,-1),cp1));
  BOOST_TEST(is_equiv  (ck1(xyz(0,0,0)),TupleMaker::create(0,0,0),cp1,eq1));
  BOOST_TEST(is_less   (ck1(xyz(0,0,0)),TupleMaker::create(0,0,1),cp1,eq1));
  BOOST_TEST(is_greater(ck1(xyz(0,0,0)),TupleMaker::create(0,0,-1),cp1,eq1));

  typedef composite_key_result_greater<ckey_t1::result_type> ckey_comp_t2;

  ckey_comp_t2 cp2;

  BOOST_TEST(is_equiv  (ck1(xyz(0,0,0)),ck2(xyz(0,0,0)),cp2));
  BOOST_TEST(is_greater(ck1(xyz(0,0,1)),ck2(xyz(0,1,0)),cp2));
  BOOST_TEST(is_less   (ck1(xyz(0,0,1)),ck2(xyz(0,0,0)),cp2));

  BOOST_TEST(is_equiv  (ck1(xyz(0,0,0)),TupleMaker::create(0),cp2));
  BOOST_TEST(is_greater(ck1(xyz(0,0,0)),TupleMaker::create(1),cp2));
  BOOST_TEST(is_less   (ck1(xyz(0,0,0)),TupleMaker::create(-1),cp2));
  BOOST_TEST(is_equiv  (ck1(xyz(0,0,0)),TupleMaker::create(0,0),cp2));
  BOOST_TEST(is_greater(ck1(xyz(0,0,0)),TupleMaker::create(0,1),cp2));
  BOOST_TEST(is_less   (ck1(xyz(0,0,0)),TupleMaker::create(0,-1),cp2));
  BOOST_TEST(is_equiv  (ck1(xyz(0,0,0)),TupleMaker::create(0,0,0),cp2));
  BOOST_TEST(is_greater(ck1(xyz(0,0,0)),TupleMaker::create(0,0,1),cp2));
  BOOST_TEST(is_less   (ck1(xyz(0,0,0)),TupleMaker::create(0,0,-1),cp2));

  typedef composite_key_equal_to<
    modulo_equal,
    modulo_equal,
    std::equal_to<int>,
    std::equal_to<int>
  > ckey_eq_t2;

  ckey_eq_t2 eq2(
    boost::make_tuple(
      modulo_equal(2),
      modulo_equal(3),
      std::equal_to<int>(),
      std::equal_to<int>()));
  ckey_eq_t2 eq3(eq2);
  ckey_eq_t2 eq4(
    get<0>(eq3.key_eqs()),
    get<1>(eq3.key_eqs()));
  ckey_eq_t2 eq5(
    std::make_tuple(
      modulo_equal(2),
      modulo_equal(3),
      std::equal_to<int>(),
      std::equal_to<int>()));

  eq3=eq4; /* prevent unused var */
  eq4=eq3; /* prevent unused var */
  eq5=eq4; /* prevent unused var */

  BOOST_TEST( eq2(ck1(xyz(0,0,0)),ck1(xyz(0,0,0))));
  BOOST_TEST(!eq2(ck1(xyz(0,1,0)),ck1(xyz(0,0,0))));
  BOOST_TEST(!eq2(ck1(xyz(0,2,0)),ck1(xyz(0,0,0))));
  BOOST_TEST( eq2(ck1(xyz(0,3,0)),ck1(xyz(0,0,0))));
  BOOST_TEST(!eq2(ck1(xyz(1,0,0)),ck1(xyz(0,0,0))));
  BOOST_TEST(!eq2(ck1(xyz(1,1,0)),ck1(xyz(0,0,0))));
  BOOST_TEST(!eq2(ck1(xyz(1,2,0)),ck1(xyz(0,0,0))));
  BOOST_TEST(!eq2(ck1(xyz(1,3,0)),ck1(xyz(0,0,0))));
  BOOST_TEST( eq2(ck1(xyz(2,0,0)),ck1(xyz(0,0,0))));
  BOOST_TEST(!eq2(ck1(xyz(2,1,0)),ck1(xyz(0,0,0))));
  BOOST_TEST(!eq2(ck1(xyz(2,2,0)),ck1(xyz(0,0,0))));
  BOOST_TEST( eq2(ck1(xyz(2,3,0)),ck1(xyz(0,0,0))));

  BOOST_TEST( eq2(TupleMaker::create(0,0,0),ck1(xyz(0,0,0))));
  BOOST_TEST(!eq2(ck1(xyz(0,1,0))          ,TupleMaker::create(0,0,0)));
  BOOST_TEST(!eq2(TupleMaker::create(0,2,0),ck1(xyz(0,0,0))));
  BOOST_TEST( eq2(ck1(xyz(0,3,0))          ,TupleMaker::create(0,0,0)));
  BOOST_TEST(!eq2(TupleMaker::create(1,0,0),ck1(xyz(0,0,0))));
  BOOST_TEST(!eq2(ck1(xyz(1,1,0))          ,TupleMaker::create(0,0,0)));
  BOOST_TEST(!eq2(TupleMaker::create(1,2,0),ck1(xyz(0,0,0))));
  BOOST_TEST(!eq2(ck1(xyz(1,3,0))          ,TupleMaker::create(0,0,0)));
  BOOST_TEST( eq2(TupleMaker::create(2,0,0),ck1(xyz(0,0,0))));
  BOOST_TEST(!eq2(ck1(xyz(2,1,0))          ,TupleMaker::create(0,0,0)));
  BOOST_TEST(!eq2(TupleMaker::create(2,2,0),ck1(xyz(0,0,0))));
  BOOST_TEST( eq2(ck1(xyz(2,3,0))          ,TupleMaker::create(0,0,0)));

  typedef composite_key_compare<
    std::less<int>,
    std::greater<int>, /* order reversed */
    std::less<int>
  > ckey_comp_t3;

  ckey_comp_t3 cp3;
  ckey_comp_t3 cp4(cp3);
  ckey_comp_t3 cp5(
    boost::make_tuple(
      std::less<int>(),
      std::greater<int>(),
      std::less<int>()));
  ckey_comp_t3 cp6(get<0>(cp3.key_comps()));
  ckey_comp_t3 cp7(
    std::make_tuple(
      std::less<int>(),
      std::greater<int>(),
      std::less<int>()));

  cp4=cp5; /* prevent unused var */
  cp5=cp6; /* prevent unused var */
  cp6=cp4; /* prevent unused var */
  cp7=cp4; /* prevent unused var */

  BOOST_TEST(is_equiv  (ck1(xyz(0,0,0)),ck2(xyz(0,0,0)),cp3));
  BOOST_TEST(is_greater(ck1(xyz(0,0,1)),ck2(xyz(0,1,0)),cp3));
  BOOST_TEST(is_greater(ck1(xyz(0,0,1)),ck2(xyz(0,0,0)),cp3));

  BOOST_TEST(is_equiv  (ck1(xyz(0,0,0)),TupleMaker::create(0),cp3));
  BOOST_TEST(is_less   (ck1(xyz(0,0,0)),TupleMaker::create(1),cp3));
  BOOST_TEST(is_greater(ck1(xyz(0,0,0)),TupleMaker::create(-1),cp3));
  BOOST_TEST(is_equiv  (ck1(xyz(0,0,0)),TupleMaker::create(0,0),cp3));
  BOOST_TEST(is_less   (ck1(xyz(0,0,0)),TupleMaker::create(0,-1),cp3));
  BOOST_TEST(is_greater(ck1(xyz(0,0,0)),TupleMaker::create(0,1),cp3));
  BOOST_TEST(is_equiv  (ck1(xyz(0,0,0)),TupleMaker::create(0,0,0),cp3));
  BOOST_TEST(is_less   (ck1(xyz(0,0,0)),TupleMaker::create(0,0,1),cp3));
  BOOST_TEST(is_greater(ck1(xyz(0,0,0)),TupleMaker::create(0,0,-1),cp3));

  typedef composite_key<
    xyz,
    BOOST_MULTI_INDEX_MEMBER(xyz,int,y), /* members reversed */
    BOOST_MULTI_INDEX_MEMBER(xyz,int,x)
  > ckey_t2;

  ckey_t2 ck6;

  BOOST_TEST(is_equiv  (ck1(xyz(0,0,1)),ck6(xyz(0,0,0))));
  BOOST_TEST(is_less   (ck1(xyz(0,0,0)),ck6(xyz(-1,1,0))));
  BOOST_TEST(is_greater(ck1(xyz(0,0,0)),ck6(xyz(1,-1,0))));

  BOOST_TEST(is_equiv  (ck1(xyz(0,0,1)),ck6(xyz(0,0,0)),cp1));
  BOOST_TEST(is_less   (ck1(xyz(0,0,0)),ck6(xyz(-1,1,0)),cp1));
  BOOST_TEST(is_greater(ck1(xyz(0,0,0)),ck6(xyz(1,-1,0)),cp1));

  BOOST_TEST(is_equiv  (ck1(xyz(0,0,1)),ck6(xyz(0,0,0)),cp2));
  BOOST_TEST(is_greater(ck1(xyz(0,0,0)),ck6(xyz(-1,1,0)),cp2));
  BOOST_TEST(is_less   (ck1(xyz(0,0,0)),ck6(xyz(1,-1,0)),cp2));

  BOOST_TEST(is_equiv  (ck1(xyz(0,0,1)),ck6(xyz(0,0,0)),cp3));
  BOOST_TEST(is_less   (ck1(xyz(0,0,0)),ck6(xyz(-1,1,0)),cp3));
  BOOST_TEST(is_greater(ck1(xyz(0,0,0)),ck6(xyz(1,-1,0)),cp3));

  typedef multi_index_container<
    xyz,
    indexed_by<
      hashed_unique<ckey_t1>
    >
  > indexed_t2;

  indexed_t2 mc2;
  mc2.insert(xyz(0,0,0));
  mc2.insert(xyz(0,0,1));
  mc2.insert(xyz(0,1,0));
  mc2.insert(xyz(0,1,1));
  mc2.insert(xyz(1,0,0));
  mc2.insert(xyz(1,0,1));
  mc2.insert(xyz(1,1,0));
  mc2.insert(xyz(1,1,1));
  mc2.insert(xyz(0,0,0));
  mc2.insert(xyz(0,0,1));
  mc2.insert(xyz(0,1,0));
  mc2.insert(xyz(0,1,1));
  mc2.insert(xyz(1,0,0));
  mc2.insert(xyz(1,0,1));
  mc2.insert(xyz(1,1,0));
  mc2.insert(xyz(1,1,1));

  BOOST_TEST(mc2.size()==8);
  BOOST_TEST(mc2.find(TupleMaker::create(0,0,1))->z==1);
  BOOST_TEST(ck1(*(mc2.find(TupleMaker::create(1,0,1))))==
             TupleMaker::create(1,0,1));

  typedef composite_key<
    xystr,
    BOOST_MULTI_INDEX_MEMBER(xystr,std::string,str),
    BOOST_MULTI_INDEX_MEMBER(xystr,int,x),
    BOOST_MULTI_INDEX_MEMBER(xystr,int,y)
  > ckey_t3;

  ckey_t3 ck7;

  typedef composite_key_hash<
    boost::hash<std::string>,
    boost::hash<int>,
    boost::hash<int>
  > ckey_hash_t;

  ckey_hash_t ch1;
  ckey_hash_t ch2(ch1);
  ckey_hash_t ch3(
    boost::make_tuple(
      boost::hash<std::string>(),
      boost::hash<int>(),
      boost::hash<int>()));
  ckey_hash_t ch4(get<0>(ch1.key_hash_functions()));
  ckey_hash_t ch5(
    std::make_tuple(
      boost::hash<std::string>(),
      boost::hash<int>(),
      boost::hash<int>()));

  ch2=ch3; /* prevent unused var */
  ch3=ch4; /* prevent unused var */
  ch4=ch2; /* prevent unused var */
  ch5=ch2; /* prevent unused var */

  BOOST_TEST(
    ch1(ck7(xystr(0,0,"hello")))==
    ch1(TupleMaker::create(std::string("hello"),0,0)));
  BOOST_TEST(
    ch1(ck7(xystr(4,5,"world")))==
    ch1(TupleMaker::create(std::string("world"),4,5)));

  typedef boost::hash<composite_key_result<ckey_t3> > ckeyres_hash_t;

  ckeyres_hash_t crh;

  BOOST_TEST(
    ch1(ck7(xystr(0,0,"hello")))==crh(ck7(xystr(0,0,"hello"))));
  BOOST_TEST(
    ch1(ck7(xystr(4,5,"world")))==crh(ck7(xystr(4,5,"world"))));
}

void test_composite_key_with_long_tuple()
{
  /* length greater than what boost::tuple allows */

  typedef composite_key<
    xystr,
    BOOST_MULTI_INDEX_MEMBER(xystr,std::string,str),
    BOOST_MULTI_INDEX_MEMBER(xystr,int,x),
    BOOST_MULTI_INDEX_MEMBER(xystr,int,y),
    BOOST_MULTI_INDEX_MEMBER(xystr,std::string,str),
    BOOST_MULTI_INDEX_MEMBER(xystr,int,x),
    BOOST_MULTI_INDEX_MEMBER(xystr,int,y),
    BOOST_MULTI_INDEX_MEMBER(xystr,std::string,str),
    BOOST_MULTI_INDEX_MEMBER(xystr,int,x),
    BOOST_MULTI_INDEX_MEMBER(xystr,int,y),
    BOOST_MULTI_INDEX_MEMBER(xystr,std::string,str),
    BOOST_MULTI_INDEX_MEMBER(xystr,int,x),
    BOOST_MULTI_INDEX_MEMBER(xystr,int,y)
  > ckey_t;

  ckey_t ck{
    BOOST_MULTI_INDEX_MEMBER(xystr,std::string,str)(),
    BOOST_MULTI_INDEX_MEMBER(xystr,int,x)(),
    BOOST_MULTI_INDEX_MEMBER(xystr,int,y)(),
    BOOST_MULTI_INDEX_MEMBER(xystr,std::string,str)(),
    BOOST_MULTI_INDEX_MEMBER(xystr,int,x)(),
    BOOST_MULTI_INDEX_MEMBER(xystr,int,y)(),
    BOOST_MULTI_INDEX_MEMBER(xystr,std::string,str)(),
    BOOST_MULTI_INDEX_MEMBER(xystr,int,x)(),
    BOOST_MULTI_INDEX_MEMBER(xystr,int,y)(),
    BOOST_MULTI_INDEX_MEMBER(xystr,std::string,str)(),
    BOOST_MULTI_INDEX_MEMBER(xystr,int,x)(),
    BOOST_MULTI_INDEX_MEMBER(xystr,int,y)()
  };

  typedef composite_key_equal_to<
    std::equal_to<std::string>,std::equal_to<int>,std::equal_to<int>,
    std::equal_to<std::string>,std::equal_to<int>,std::equal_to<int>,
    std::equal_to<std::string>,std::equal_to<int>,std::equal_to<int>,
    std::equal_to<std::string>,std::equal_to<int>,std::equal_to<int>
  > ceq_t;

  ceq_t eq{
    std::equal_to<std::string>(),std::equal_to<int>(),std::equal_to<int>(),
    std::equal_to<std::string>(),std::equal_to<int>(),std::equal_to<int>(),
    std::equal_to<std::string>(),std::equal_to<int>(),std::equal_to<int>(),
    std::equal_to<std::string>(),std::equal_to<int>(),std::equal_to<int>()
  };

  typedef composite_key_compare<
    std::less<std::string>,std::less<int>,std::less<int>,
    std::less<std::string>,std::less<int>,std::less<int>,
    std::less<std::string>,std::less<int>,std::less<int>,
    std::less<std::string>,std::less<int>,std::less<int>
  > clt_t;

  clt_t lt{
    std::less<std::string>(),std::less<int>(),std::less<int>(),
    std::less<std::string>(),std::less<int>(),std::less<int>(),
    std::less<std::string>(),std::less<int>(),std::less<int>(),
    std::less<std::string>(),std::less<int>(),std::less<int>()
  };

  typedef composite_key_hash<
    boost::hash<std::string>,boost::hash<int>,boost::hash<int>,
    boost::hash<std::string>,boost::hash<int>,boost::hash<int>,
    boost::hash<std::string>,boost::hash<int>,boost::hash<int>,
    boost::hash<std::string>,boost::hash<int>,boost::hash<int>
  > ch_t;

  ch_t ch{
    boost::hash<std::string>(),boost::hash<int>(),boost::hash<int>(),
    boost::hash<std::string>(),boost::hash<int>(),boost::hash<int>(),
    boost::hash<std::string>(),boost::hash<int>(),boost::hash<int>(),
    boost::hash<std::string>(),boost::hash<int>(),boost::hash<int>()
  };

  xystr v{0,1,""};

  BOOST_TEST(eq(ck(v),std::make_tuple("",0,1,"",0,1,"",0,1,"",0,1)));
  BOOST_TEST(!lt(std::make_tuple("",0,1,"",0,1,"",0,1,"",0,1),ck(v)));
  BOOST_TEST((ch(ck(v))==ch(std::make_tuple("",0,1,"",0,1,"",0,1,"",0,1))));
}

void test_composite_key()
{
  test_composite_key_template<boost_tuple_maker>();
  test_composite_key_template<std_tuple_maker>();
  test_composite_key_with_long_tuple();
}

/* Copyright 2024 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#include "test_fixed_variant.hpp"

#include <boost/config.hpp>
#include <boost/container_hash/hash.hpp>
#include <boost/core/addressof.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/detail/workaround.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/integer_sequence.hpp>
#include <boost/mp11/tuple.hpp>
#include <boost/poly_collection/detail/fixed_variant.hpp>
#include <utility>
#include "test_utilities.hpp"

using namespace test_utilities;

struct non_copyable
{
  constexpr non_copyable(int=0){};
  non_copyable(const non_copyable&)=delete;
  non_copyable(non_copyable&&)=default;

  constexpr bool operator==(const non_copyable&)const{return true;}
  constexpr bool operator!=(const non_copyable&)const{return false;}
  constexpr bool operator<(const non_copyable&)const{return false;}
  constexpr bool operator<=(const non_copyable&)const{return true;}
  constexpr bool operator>(const non_copyable&)const{return false;}
  constexpr bool operator>=(const non_copyable&)const{return true;}
};

std::size_t hash_value(const non_copyable&){return 0;}

struct get_addresses
{
  using result_type=std::pair<const void*,const void*>;

  template<typename T,typename Q>
  result_type operator()(const T& x,const Q& y)const{return {&x,&y};}
};

enum cref_qualifier_value
{
  plain_ref=0,
  const_ref,
  rvalue_ref,
  const_rvalue_ref,
};

template<typename T> cref_qualifier_value cref_qualifier(T&)
  {return plain_ref;}
template<typename T> cref_qualifier_value cref_qualifier(const T&)
  {return const_ref;}
template<typename T> cref_qualifier_value cref_qualifier(T&&)
  {return rvalue_ref;}
template<typename T> cref_qualifier_value cref_qualifier(const T&&)
  {return const_rvalue_ref;}

struct get_cref_qualifiers
{
  using result_type=std::pair<cref_qualifier_value,cref_qualifier_value>;

  template<typename T,typename Q>
  result_type operator()(T&& x,Q&& y)const
  {
    return {
      cref_qualifier(std::forward<T>(x)),cref_qualifier(std::forward<Q>(y))
    };
  }
};

struct hasher_helper
{
  std::size_t& seed;

  template<typename T>
  void operator()(const T& x){boost::hash_combine(seed,x);}
};

struct hasher
{
  hasher(std::size_t seed_=0):seed{seed_}{}

  template<typename... Ts>
  std::size_t operator()(const Ts&... xs)const
  {
    std::size_t res=seed;
    boost::mp11::tuple_for_each(
      std::forward_as_tuple(xs...),hasher_helper{res});
    return res;
  }

  std::size_t seed;
};

template<typename T,std::size_t N>
using iota_tuple=boost::mp11::mp_repeat_c<std::tuple<T>,N>;

template<typename T,std::size_t... Is>
iota_tuple<T,sizeof...(Is)> make_iota_tuple(boost::mp11::index_sequence<Is...>)
{
  return iota_tuple<T,sizeof...(Is)>{T{Is}...};
}

template<typename T,std::size_t N>
iota_tuple<T,N> make_iota_tuple()
{
  return make_iota_tuple<T>(boost::mp11::make_index_sequence<N>{});
}

template<typename V>
struct visitor_by_index
{
  V&& x;

  template<typename... Fs>
  auto operator()(Fs&&... fs)->
    decltype(visit_by_index(std::forward<V>(x),std::forward<Fs>(fs)...))
  {
    return visit_by_index(std::forward<V>(x),std::forward<Fs>(fs)...);
  }
};

template<typename V>
struct void_visitor_by_index
{
  V&& x;

  template<typename... Fs>
  void operator()(Fs&&... fs)
  {
    using boost::poly_collection::visit_by_index;
    return visit_by_index<void>(std::forward<V>(x),std::forward<Fs>(fs)...);
  }
};

#if BOOST_WORKAROUND(BOOST_GCC_VERSION,<50000)
template<typename Q,typename V>
struct get_
{
  V& v;

  void operator()()
  {
    using namespace boost::poly_collection;
    (void)get<Q>(v);
  }
};
#endif

template<typename V,typename T>
void test_fixed_variant_for()
{
  using namespace boost::poly_collection;
  static constexpr std::size_t I=boost::mp11::mp_find<V,T>::value;
  static constexpr std::size_t J=I!=1?1:2;
  using Q=boost::mp11::mp_at_c<V,J>;

  fixed_variant_impl::fixed_variant_closure<T,V> x{T(0)},y{T(1)};
  fixed_variant_impl::fixed_variant_closure<Q,V> z{Q(2)};
  V                                              &v=x,&w=y;
  const V                                        &cv=v,&cw=w,&cu=z;

  BOOST_TEST(cv.index()==I);
  BOOST_TEST(!cv.valueless_by_exception());

  BOOST_TEST((cv==cw) == (x==y));
  BOOST_TEST((cv!=cw) == (x!=y));
  BOOST_TEST((cv< cw) == (x< y));
  BOOST_TEST((cv<=cw) == (x<=y));
  BOOST_TEST((cv> cw) == (x> y));
  BOOST_TEST((cv>=cw) == (x>=y));
  BOOST_TEST((cv==cu) == (I==J));
  BOOST_TEST((cv!=cu) == (I!=J));
  BOOST_TEST((cv< cu) == (I< J));
  BOOST_TEST((cv<=cu) == (I<=J));
  BOOST_TEST((cv> cu) == (I> J));
  BOOST_TEST((cv>=cu) == (I>=J));

  BOOST_TEST_TRAIT_TRUE((
    std::is_base_of<boost::mp11::mp_size<V>,variant_size<V>>));
  BOOST_TEST_TRAIT_TRUE((
    std::is_base_of<boost::mp11::mp_size<V>,variant_size<const V>>));

#ifndef BOOST_NO_CXX14_VARIABLE_TEMPLATES
  BOOST_TEST_TRAIT_TRUE((std::integral_constant<
    bool,boost::mp11::mp_size<V>::value==variant_size_v<V>>));
  BOOST_TEST_TRAIT_TRUE((std::integral_constant<
    bool,boost::mp11::mp_size<V>::value==variant_size_v<const V>>));
#endif

  BOOST_TEST_TRAIT_TRUE((
    std::is_same<typename variant_alternative<I,V>::type,T>));
  BOOST_TEST_TRAIT_TRUE((
    std::is_same<typename variant_alternative<I,const V>::type,const T>));
  BOOST_TEST_TRAIT_TRUE((
    std::is_same<typename variant_alternative<I,V&>::type,T&>));
  BOOST_TEST_TRAIT_TRUE((
    std::is_same<typename variant_alternative<I,const V&&>::type,const T&&>));

  BOOST_TEST_TRAIT_TRUE((
    std::is_same<variant_alternative_t<I,V>,T>));
  BOOST_TEST_TRAIT_TRUE((
    std::is_same<variant_alternative_t<I,const V>,const T>));
  BOOST_TEST_TRAIT_TRUE((
    std::is_same<variant_alternative_t<I,V&>,T&>));
  BOOST_TEST_TRAIT_TRUE((
    std::is_same<variant_alternative_t<I,const V&&>,const T&&>));

  BOOST_TEST((
    visit(get_addresses{},v,w)==get_addresses::result_type{&x,&y}));

#if BOOST_WORKAROUND(BOOST_GCC_VERSION,<40900)
  /*  call of overloaded 'cref_qualifier(...&)' is ambiguous */
#else
  BOOST_TEST((
    visit(get_cref_qualifiers{},v,cv)==
    get_cref_qualifiers::result_type{plain_ref,const_ref}));
  BOOST_TEST((
    visit(get_cref_qualifiers{},std::move(v),std::move(cv))==
    get_cref_qualifiers::result_type{rvalue_ref,const_rvalue_ref}));
#endif

  BOOST_TEST(
    visit(hasher{},cv,cw,cu)==hasher{}(x.value,y.value,z.value));
  BOOST_TEST(
    visit(hasher{},cv,cw,cv,cu)==hasher{}(x.value,y.value,x.value,z.value));

  visit<void>(hasher{});
  visit<void>(hasher{},cv);
  visit<void>(hasher{},cv,cw);
  visit<void>(hasher{},cv,cw,cu);

  BOOST_TEST(
    boost::mp11::tuple_apply(
      visitor_by_index<const V&>{cv},
      make_iota_tuple<hasher,boost::mp11::mp_size<V>::value>())==
    hasher{I}(x.value));

  boost::mp11::tuple_apply(
    void_visitor_by_index<const V&>{cv},
    make_iota_tuple<hasher,boost::mp11::mp_size<V>::value>());

  BOOST_TEST(holds_alternative<T>(cv));
  BOOST_TEST(!holds_alternative<Q>(cv));

  BOOST_TEST(&get<I>(v)==&x.value);
  BOOST_TEST(&get<I>(cv)==&x.value);
  BOOST_TEST(get<I>(std::move(v))==x.value);
  BOOST_TEST(get<I>(std::move(cv))==x.value);
  check_throw<bad_variant_access>([&]{(void)get<J>(v);});

  BOOST_TEST(&get<T>(v)==&x.value);
  BOOST_TEST(&get<T>(cv)==&x.value);
  BOOST_TEST(get<T>(std::move(v))==x.value);
  BOOST_TEST(get<T>(std::move(cv))==x.value);
#if BOOST_WORKAROUND(BOOST_GCC_VERSION,<50000)
  /* https://godbolt.org/z/jcfKb755o */
  check_throw<bad_variant_access>(get_<Q,V>{v});
#else
  check_throw<bad_variant_access>([&]{(void)get<Q>(v);});
#endif

  BOOST_TEST(get_if<I>((V*)nullptr)==nullptr);
  BOOST_TEST(get_if<I>(&v)==&x.value);
  BOOST_TEST(get_if<I>(&cv)==&x.value);
  BOOST_TEST(get_if<J>(&v)==nullptr);

  BOOST_TEST(get_if<T>((V*)nullptr)==nullptr);
  BOOST_TEST(get_if<T>(&v)==&x.value);
  BOOST_TEST(get_if<T>(&cv)==&x.value);
  BOOST_TEST(get_if<Q>(&v)==nullptr);

  BOOST_TEST_TRAIT_TRUE((
    std::is_same<decltype(get<I>(v)),T&>));
  BOOST_TEST_TRAIT_TRUE((
    std::is_same<decltype(get<I>(cv)),const T&>));
  BOOST_TEST_TRAIT_TRUE((
    std::is_same<decltype(get<I>(std::move(v))),T&&>));
  BOOST_TEST_TRAIT_TRUE((
    std::is_same<decltype(get<I>(std::move(cv))),const T&&>));

  BOOST_TEST_TRAIT_TRUE((
    std::is_same<decltype(get<T>(v)),T&>));
  BOOST_TEST_TRAIT_TRUE((
    std::is_same<decltype(get<T>(cv)),const T&>));
  BOOST_TEST_TRAIT_TRUE((
    std::is_same<decltype(std::move(get<T>(v))),T&&>));
  BOOST_TEST_TRAIT_TRUE((
    std::is_same<decltype(std::move(get<T>(cv))),const T&&>));

  BOOST_TEST_TRAIT_TRUE((
    std::is_same<decltype(get_if<I>(&v)),T*>));
  BOOST_TEST_TRAIT_TRUE((
    std::is_same<decltype(get_if<I>(&cv)),const T*>));

  BOOST_TEST_TRAIT_TRUE((
    std::is_same<decltype(get_if<T>(&v)),T*>));
  BOOST_TEST_TRAIT_TRUE((
    std::is_same<decltype(get_if<T>(&cv)),const T*>));
}

template<typename... Ts>
void test_fixed_variant()
{
  using variant_type=boost::poly_collection::fixed_variant_impl::
    fixed_variant<Ts...>;

  do_((test_fixed_variant_for<variant_type,Ts>(),0)...);
}

void test_fixed_variant()
{
  test_fixed_variant<int,char,non_copyable>();
}

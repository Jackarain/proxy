/* Copyright 2016-2024 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#include "test_insertion.hpp"

#include <algorithm>
#include <boost/core/lightweight_test.hpp>
#include <boost/variant2/variant.hpp>
#include <numeric>
#include <vector>
#include "any_types.hpp"
#include "base_types.hpp"
#include "function_types.hpp"
#include "variant_types.hpp"
#include "test_utilities.hpp"

#if !defined(BOOST_NO_CXX17_HDR_VARIANT)
#include <variant>
#endif

using namespace test_utilities;

template<
  typename PolyCollection,typename ValueFactory,typename... Types,
  typename std::enable_if<
    !is_closed_collection<PolyCollection>::value>::type* =nullptr
>
void test_insertion_throw()
{
  {
    using unregistered_type=boost::poly_collection::unregistered_type;
    using type=first_of<constraints<is_copy_constructible>,Types...>;

    PolyCollection p,p2;
    ValueFactory   v;

    p2.insert(v.template make<type>());
    check_throw<unregistered_type>(
      [&]{p.insert(*p2.begin());},
      [&]{p.insert(p.end(),*p2.begin());},
      [&]{p.insert(p.cend(),*p2.begin());},
      [&]{p.insert(
        external_iterator(p2.begin()),external_iterator(p2.end()));},
      [&]{p.insert(
        p.end(),
        external_iterator(p2.begin()),external_iterator(p2.end()));},
      [&]{p.insert(
        p.cend(),
        external_iterator(p2.begin()),external_iterator(p2.end()));});
  }
  {
    using not_copy_constructible=
      boost::poly_collection::not_copy_constructible;
    using type=first_of<constraints<is_not_copy_constructible>,Types...>;

    PolyCollection p,p2;
    ValueFactory   v;

    register_types<type>(p);
    p2.insert(v.template make<type>());
    auto p2b=external_iterator(p2.begin()),
         p2e=external_iterator(p2.end());
    auto p2lb=external_iterator(p2.template begin<type>()),
         p2le=external_iterator(p2.template end<type>());
    check_throw<not_copy_constructible>(
      [&]{p.insert(*p2.begin());},
      [&]{p.insert(p.end(),*p2.begin());},
      [&]{p.insert(p.cend(),*p2.cbegin());},
      [&]{p.insert(p.end(typeid_<type>(p)),*p2.begin());},
      [&]{p.insert(p.cend(typeid_<type>(p)),*p2.begin());},
      [&]{p.insert(p.template end<type>(),*p2.begin());},
      [&]{p.insert(p.template cend<type>(),*p2.begin());},
      [&]{p.insert(p2b,p2e);},
      [&]{p.insert(p2lb,p2le);},
      [&]{p.insert(p2.begin(),p2.end());},
      [&]{p.insert(p2.begin(typeid_<type>(p)),p2.end(typeid_<type>(p)));},
      [&]{p.insert(p2.template begin<type>(),p2.template end<type>());},
      [&]{p.insert(p.end(),p2b,p2e);},
      [&]{p.insert(p.end(),p2lb,p2le);},
      [&]{p.insert(p.end(),p2.begin(),p2.end());},
      [&]{p.insert(
        p.end(),p2.begin(typeid_<type>(p)),p2.end(typeid_<type>(p)));},
      [&]{p.insert(
        p.end(),p2.template begin<type>(),p2.template end<type>());},
      [&]{p.insert(p.cend(),p2b,p2e);},
      [&]{p.insert(p.cend(),p2lb,p2le);},
      [&]{p.insert(p.cend(),p2.begin(),p2.end());},
      [&]{p.insert(
        p.cend(),p2.begin(typeid_<type>(p)),p2.end(typeid_<type>(p)));},
      [&]{p.insert(
        p.cend(),p2.template begin<type>(),p2.template end<type>());},
      [&]{p.insert(p.end(typeid_<type>(p)),p2b,p2e);},
      [&]{p.insert(p.cend(typeid_<type>(p)),p2b,p2e);},
      [&]{p.insert(p.template end<type>(),p2b,p2e);},
      [&]{p.insert(p.template cend<type>(),p2b,p2e);});
  }
}

template<
  typename PolyCollection,typename ValueFactory,typename... Types,
  typename std::enable_if<
    is_closed_collection<PolyCollection>::value>::type* =nullptr
>
void test_insertion_throw()
{
}

template<typename PolyCollection,typename ValueFactory,typename... Types>
void test_insertion()
{
  test_insertion_throw<PolyCollection,ValueFactory,Types...>();
  {
    PolyCollection p;
    ValueFactory   v;

    fill<constraints<>,Types...>(p,v,2);

    do_((BOOST_TEST(
      is_last(
        p,typeid_<Types>(p),
        p.insert(constref_if_copy_constructible(v.template make<Types>())))
    ),0)...);
  }
  {
    PolyCollection p;
    ValueFactory   v;

    fill<constraints<>,Types...>(p,v,2);

    auto& info=p.segment_traversal().begin()->type_info();
    do_((BOOST_TEST(
      info==typeid_<Types>(p)?
        is_first(
          p,typeid_<Types>(p),
          p.insert(
            p.cbegin(),
            constref_if_copy_constructible(v.template make<Types>()))):
        is_last(
          p,typeid_<Types>(p),
          p.insert(
            p.cbegin(),
            constref_if_copy_constructible(v.template make<Types>())))
    ),0)...);

    do_((BOOST_TEST(
      is_first(
        p,typeid_<Types>(p),
        p.insert(
          p.cbegin(typeid_<Types>(p)),
          constref_if_copy_constructible(v.template make<Types>())))
    ),0)...);

    do_((BOOST_TEST(
      is_first<Types>(
        p,
        p.insert(
          p.template cbegin<Types>(),
          constref_if_copy_constructible(v.template make<Types>())))
    ),0)...);
  }
  {
    PolyCollection p,p2;
    ValueFactory   v;

    register_types<Types...>(p);
    register_types<Types...>(p2);
    fill<
      constraints<is_copy_constructible,is_equality_comparable>,
      Types...
    >(p2,v,2);

    p.insert(external_iterator(p2.begin()),external_iterator(p2.end()));
    BOOST_TEST(p==p2);
    p.clear();

    p.insert(p2.begin(),p2.end());
    BOOST_TEST(p==p2);
    p.clear();

    p.insert(p2.cbegin(),p2.cend());
    BOOST_TEST(p==p2);
    p.clear();

    for(auto s:p2.segment_traversal()){
      p.insert(s.begin(),s.end());
      BOOST_TEST(p.size()==p2.size(s.type_info()));
      p.clear();

      p.insert(s.cbegin(),s.cend());
      BOOST_TEST(p.size()==p2.size(s.type_info()));
      p.clear();
    }

    do_((
      p.insert(
        external_iterator(p2.template begin<Types>()),
        external_iterator(p2.template end<Types>())),
      BOOST_TEST(p.size()==p2.template size<Types>()),
      p.clear() 
    ,0)...);

    do_((
      p.insert(p2.template begin<Types>(),p2.template end<Types>()),
      BOOST_TEST(p.size()==p2.template size<Types>()),
      p.clear() 
    ,0)...);

    do_((
      p.insert(p2.template cbegin<Types>(),p2.template cend<Types>()),
      BOOST_TEST(p.size()==p2.template size<Types>()),
      p.clear() 
    ,0)...);
  }
  {
    PolyCollection p,p1,p2;
    ValueFactory   v;

    register_types<Types...>(p2);
    fill<
      constraints<is_copy_constructible,is_equality_comparable>,
      Types...
    >(p1,v,2);
    fill<
      constraints<is_copy_constructible,is_equality_comparable>,
      Types...
    >(p2,v,2);

    auto remove_original=[](PolyCollection& p)
    {
      bool first=true;
      for(auto s:p.segment_traversal()){
        if(s.begin()!=s.end()){
          if(first)p.erase(s.end()-2,s.end()),first=false;
          else     p.erase(s.begin(),s.begin()+2);
        }
      }
    };

    p=p1;
    p.insert(
      p.begin(),external_iterator(p2.begin()),external_iterator(p2.end()));
    remove_original(p);
    BOOST_TEST(p==p2);

    p=p1;
    p.insert(p.begin(),p2.begin(),p2.end());
    remove_original(p);
    BOOST_TEST(p==p2);

    p=p1;
    p.insert(p.begin(),p2.cbegin(),p2.cend());
    remove_original(p);
    BOOST_TEST(p==p2);

    p=p1;
    for(auto s:p2.segment_traversal())p.insert(p.begin(),s.begin(),s.end());
    remove_original(p);
    BOOST_TEST(p==p2);

    p=p1;
    for(auto s:p2.segment_traversal())p.insert(p.begin(),s.cbegin(),s.cend());
    remove_original(p);
    BOOST_TEST(p==p2);

    p=p1;
    do_((p.insert(
      p.begin(),
      external_iterator(p2.template begin<Types>()),
      external_iterator(p2.template end<Types>())),0)...);
    remove_original(p);
    BOOST_TEST(p==p2);

    p=p1;
    do_((p.insert(
      p.begin(),p2.template begin<Types>(),p2.template end<Types>()),0)...);
    remove_original(p);
    BOOST_TEST(p==p2);

    p=p1;
    do_((p.insert(
      p.begin(),p2.template cbegin<Types>(),p2.template cend<Types>()),0)...);
    remove_original(p);
    BOOST_TEST(p==p2);
  }
  {
    using type=first_of<
      constraints<is_copy_constructible,is_equality_comparable>,
      Types...
    >;

    PolyCollection p,p1,p2;
    ValueFactory   v;

    fill<constraints<>,type>(p1,v,2);
    fill<constraints<>,type>(p2,v,2);

    auto remove_original=[](PolyCollection& p)
    {
      auto it=p.segment_traversal().begin()->end();
      p.erase(it-2,it);
    };

    p=p1;
    BOOST_TEST(is_first(
      p,typeid_<type>(p),
      p.insert(
        p.begin(typeid_<type>(p)),
        external_iterator(p2.begin()),external_iterator(p2.end()))));
    remove_original(p);
    BOOST_TEST(p==p2);

    p=p1;
    BOOST_TEST(is_first(
      p,typeid_<type>(p),
      p.insert(
        p.cbegin(typeid_<type>(p)),
        external_iterator(p2.begin()),external_iterator(p2.end()))));
    remove_original(p);
    BOOST_TEST(p==p2);

    p=p1;
    BOOST_TEST(is_first<type>(
      p,
      p.insert(
        p.template begin<type>(),
        external_iterator(p2.begin()),external_iterator(p2.end()))));
    remove_original(p);
    BOOST_TEST(p==p2);

    p=p1;
    BOOST_TEST(is_first<type>(
      p,
      p.insert(
        p.template cbegin<type>(),
        external_iterator(p2.begin()),external_iterator(p2.end()))));
    remove_original(p);
    BOOST_TEST(p==p2);
  }
  {
    using type=first_of<
      constraints<
        is_constructible_from_int,is_not_copy_constructible,
        is_equality_comparable
      >,
      Types...
    >;

    PolyCollection   p;
    std::vector<int> s(10);
    ValueFactory     v;

    fill<constraints<>,type>(p,v,2);
    std::iota(s.begin(),s.end(),0);
    BOOST_TEST(is_first<type>(
      p,
      p.insert(p.template begin<type>(),s.begin(),s.end())));
    BOOST_TEST(
      std::equal(s.begin(),s.end(),p.template begin<type>(),
      [](int x,const type& y){return type{x}==y;})
    );
  }
}

void test_variant_insertion()
{
  struct move_track
  {
    move_track(int n_):n{n_}{}
    move_track(move_track&& x)noexcept:n{x.n}{x.n=0;}

    int n;
  };

  boost::variant_collection<
    boost::mp11::mp_list<int,char,double,move_track>> c1;

  c1.insert(1);
  c1.insert('a');
  c1.insert(3.1416);

  auto c2=c1;
  c1.insert(*c2.begin());
  BOOST_TEST_EQ(c1.size<int>(),2);

  boost::variant2::variant<move_track,double,char> v1('a');
  c1.insert(v1);
  BOOST_TEST_EQ(c1.size<char>(),2);

  v1.emplace<move_track>(1);
  c1.insert(std::move(v1));
  BOOST_TEST_EQ(c1.size<move_track>(),1);
  BOOST_TEST(boost::variant2::get<move_track>(v1).n==0);

#if !defined(BOOST_NO_CXX17_HDR_VARIANT)
  std::variant<int,move_track,double> v2(3.1416);
  c1.insert(v2);
  BOOST_TEST_EQ(c1.size<double>(),2);

  v2.emplace<move_track>(1);
  c1.insert(std::move(v2));
  BOOST_TEST_EQ(c1.size<move_track>(),2);
  BOOST_TEST(std::get<move_track>(v2).n==0);
#endif
}

void test_insertion()
{
  test_insertion<
    any_types::collection,auto_increment,
    any_types::t1,any_types::t2,any_types::t3,
    any_types::t4,any_types::t5>();
  test_insertion<
    base_types::collection,auto_increment,
    base_types::t1,base_types::t2,base_types::t3,
    base_types::t4,base_types::t5>();
  test_insertion<
    function_types::collection,auto_increment,
    function_types::t1,function_types::t2,function_types::t3,
    function_types::t4,function_types::t5>();
  test_insertion<
    variant_types::collection,auto_increment,
    variant_types::t1,variant_types::t2,variant_types::t3,
    variant_types::t4,variant_types::t5>();
  test_variant_insertion();
}

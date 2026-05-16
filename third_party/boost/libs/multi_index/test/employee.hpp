/* Used in Boost.MultiIndex tests.
 *
 * Copyright 2003-2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/multi_index for library home page.
 */

#ifndef BOOST_MULTI_INDEX_TEST_EMPLOYEE_HPP
#define BOOST_MULTI_INDEX_TEST_EMPLOYEE_HPP

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/ranked_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <ostream>
#include <string>
#include <utility>
#include "non_std_allocator.hpp"

struct employee
{
  int         id;
  std::string name;
  int         age;
  int         ssn;

  employee(int id_,std::string name_,int age_,int ssn_):
    id(id_),name(name_),age(age_),ssn(ssn_)
  {}

  employee(const employee& x):
    id(x.id),name(x.name),age(x.age),ssn(x.ssn)
  {}

  employee(employee&& x):
    id(x.id),name(std::move(x.name)),age(x.age),ssn(x.ssn)
  {}

  employee& operator=(const employee& x)
  {
    id=x.id;
    name=x.name;
    age=x.age;
    ssn=x.ssn;
    return *this;
  };

  employee& operator=(employee&& x)
  {
    id=x.id;
    name=std::move(x.name);
    age=x.age;
    ssn=x.ssn;
    return *this;
  }

  bool operator==(const employee& x)const
  {
    return id==x.id&&name==x.name&&age==x.age;
  }

  bool operator<(const employee& x)const
  {
    return id<x.id;
  }

  bool operator!=(const employee& x)const{return !(*this==x);}
  bool operator> (const employee& x)const{return x<*this;}
  bool operator>=(const employee& x)const{return !(*this<x);}
  bool operator<=(const employee& x)const{return !(x<*this);}

  struct comp_id
  {
    bool operator()(int x,const employee& e2)const{return x<e2.id;}
    bool operator()(const employee& e1,int x)const{return e1.id<x;}
  };

  friend std::ostream& operator<<(std::ostream& os,const employee& e)
  {
    os<<e.id<<" "<<e.name<<" "<<e.age<<std::endl;
    return os;
  }
};

struct name{};
struct by_name{};
struct age{};
struct as_inserted{};
struct ssn{};
struct randomly{};

using employee_set_indices=
  boost::multi_index::indexed_by<
    boost::multi_index::ordered_unique<
      boost::multi_index::identity<employee> >,
    boost::multi_index::hashed_non_unique<
      boost::multi_index::tag<name,by_name>,
      BOOST_MULTI_INDEX_MEMBER(employee,std::string,name)>,
    boost::multi_index::ranked_non_unique<
      boost::multi_index::tag<age>,
      BOOST_MULTI_INDEX_MEMBER(employee,int,age)>,
    boost::multi_index::sequenced<
      boost::multi_index::tag<as_inserted> >,
    boost::multi_index::hashed_unique<
      boost::multi_index::tag<ssn>,
      BOOST_MULTI_INDEX_MEMBER(employee,int,ssn)>,
    boost::multi_index::random_access<
      boost::multi_index::tag<randomly> > >;

typedef
  boost::multi_index::multi_index_container<
    employee,
    employee_set_indices,
    non_std_allocator<employee> >        employee_set;

typedef employee_set::nth_index<1>::type employee_set_by_name;

typedef boost::multi_index::index<
         employee_set,age>::type         employee_set_by_age;
typedef boost::multi_index::index<
         employee_set,as_inserted>::type employee_set_as_inserted;
typedef boost::multi_index::index<
         employee_set,ssn>::type         employee_set_by_ssn;

typedef employee_set::index<
          randomly>::type                employee_set_randomly;

#endif

/* Boost.MultiIndex test for projection capabilities.
 *
 * Copyright 2003-2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/multi_index for library home page.
 */

#include "test_projection.hpp"

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include "pre_multi_index.hpp"
#include "employee.hpp"
#include <boost/detail/lightweight_test.hpp>

using namespace boost::multi_index;

void test_projection()
{
  employee_set          es;
  es.insert(employee(0,"Joe",31,1123));
  es.insert(employee(1,"Robert",27,5601));
  es.insert(employee(2,"John",40,7889));
  es.insert(employee(3,"Albert",20,9012));
  es.insert(employee(4,"John",57,1002));

  employee_set::iterator             it,itbis;
  employee_set_by_name::iterator     it1;
  employee_set_by_age::iterator      it2;
  employee_set_as_inserted::iterator it3;
  employee_set_by_ssn::iterator      it4;
  employee_set_randomly::iterator    it5;

  BOOST_STATIC_ASSERT((boost::is_same<
    employee_set::iterator,
    nth_index_iterator<employee_set,0>::type >::value));
  BOOST_STATIC_ASSERT((boost::is_same<
    employee_set_by_name::iterator,
    nth_index_iterator<employee_set,1>::type >::value));
  BOOST_STATIC_ASSERT((boost::is_same<
    employee_set_by_age::iterator,
    employee_set::index_iterator<age>::type >::value));
  BOOST_STATIC_ASSERT((boost::is_same<
    employee_set_as_inserted::iterator,
    nth_index_iterator<employee_set,3>::type >::value));
  BOOST_STATIC_ASSERT((boost::is_same<
    employee_set_by_ssn::iterator,
    nth_index_iterator<employee_set,4>::type >::value));
  BOOST_STATIC_ASSERT((boost::is_same<
    employee_set_randomly::iterator,
    nth_index_iterator<employee_set,5>::type >::value));

  it=   es.find(employee(1,"Robert",27,5601));
  it1=  project<name>(es,it);
  it2=  project<age>(es,it1);
  it3=  project<as_inserted>(es,it2);
  it4=  project<ssn>(es,it3);
  it5=  project<randomly>(es,it4);
  itbis=es.project<0>(it5);

  BOOST_TEST(
    *it==*it1&&*it1==*it2&&*it2==*it3&&*it3==*it4&&*it4==*it5&&itbis==it);

  BOOST_TEST(project<name>(es,es.end())==get<name>(es).end());
  BOOST_TEST(project<age>(es,es.end())==get<age>(es).end());
  BOOST_TEST(project<as_inserted>(es,es.end())==get<as_inserted>(es).end());
  BOOST_TEST(project<ssn>(es,es.end())==get<ssn>(es).end());
  BOOST_TEST(project<randomly>(es,es.end())==get<randomly>(es).end());

  const employee_set& ces=es;

  employee_set::const_iterator             cit,citbis;
  employee_set_by_name::const_iterator     cit1;
  employee_set_by_age::const_iterator      cit2;
  employee_set_as_inserted::const_iterator cit3;
  employee_set_by_ssn::const_iterator      cit4;
  employee_set_randomly::const_iterator    cit5;

  BOOST_STATIC_ASSERT((boost::is_same<
    employee_set::const_iterator,
    nth_index_const_iterator<employee_set,0>::type >::value));
  BOOST_STATIC_ASSERT((boost::is_same<
    employee_set_by_name::const_iterator,
    nth_index_const_iterator<employee_set,1>::type >::value));
  BOOST_STATIC_ASSERT((boost::is_same<
    employee_set_by_age::const_iterator,
    employee_set::index_const_iterator<age>::type >::value));
  BOOST_STATIC_ASSERT((boost::is_same<
    employee_set_as_inserted::const_iterator,
    nth_index_const_iterator<employee_set,3>::type >::value));
  BOOST_STATIC_ASSERT((boost::is_same<
    employee_set_by_ssn::const_iterator,
    nth_index_const_iterator<employee_set,4>::type >::value));
  BOOST_STATIC_ASSERT((boost::is_same<
    employee_set_randomly::const_iterator,
    nth_index_const_iterator<employee_set,5>::type >::value));

  cit=   ces.find(employee(4,"John",57,1002));
  cit1=  ces.project<by_name>(cit);
  cit2=  project<age>(ces,cit1);
  cit3=  ces.project<as_inserted>(cit2);
  cit4=  project<ssn>(ces,cit3);
  cit5=  project<randomly>(ces,cit4);
  citbis=project<0>(ces,cit5);

  BOOST_TEST(
    *cit==*cit1&&*cit1==*cit2&&*cit2==*cit3&&*cit3==*cit4&&*cit4==*cit5&&
    citbis==cit);
}

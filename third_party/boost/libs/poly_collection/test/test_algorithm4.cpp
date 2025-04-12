/* Copyright 2024 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#include "test_algorithm4.hpp"

#include <boost/detail/workaround.hpp>
#include <boost/iterator/function_output_iterator.hpp>
#include <iterator>
#include <random>
#include <type_traits>
#include "variant_types.hpp"
#include "test_algorithm_impl.hpp"

template<typename T>
struct pierced_pred
{
  template<
    typename Q,
    typename std::enable_if<!std::is_same<T,Q>::value>::type* =nullptr
  >
  bool operator()(const Q&)const{return true;}

  template<
    typename Q,typename R,
    typename std::enable_if<
      !std::is_same<T,Q>::value&&!std::is_same<T,R>::value>::type* =nullptr
  >
  bool operator()(const Q&,const R&)const{return true;}
};

template<typename PolyCollection,typename T>
void test_total_restitution_algorithm()
{
  using namespace boost::poly_collection;
  using value_type=typename PolyCollection::value_type;

  PolyCollection           p;
  pierced_pred<value_type> pred;
  int                      seq[1];
  auto                     out=boost::make_function_output_iterator(pred);

#if BOOST_WORKAROUND(BOOST_MSVC,>=1910)&&BOOST_WORKAROUND(BOOST_MSVC,<1920)
/* Internal fix for https://lists.boost.org/Archives/boost/2017/06/235687.php
 * causes instantiation of std alg for value_type.
 */
#else
  (void)all_of<all_types>(p.begin(),p.end(),pred);
  (void)any_of<all_types>(p.begin(),p.end(),pred);
  (void)none_of<all_types>(p.begin(),p.end(),pred);
  (void)count_if<all_types>(p.begin(),p.end(),pred);
  (void)is_permutation<all_types>(
    p.begin(),p.end(),std::begin(seq),pred);
  (void)copy<all_types>(p.begin(),p.end(),out);
  (void)copy_if<all_types>(p.begin(),p.end(),out,pred);
  (void)rotate_copy<all_types>(p.begin(),p.begin(),p.end(),out);
  (void)move<all_types>(p.begin(),p.end(),out);
  (void)transform<all_types>(p.begin(),p.end(),out,pred);
  (void)remove_copy<all_types>(p.begin(),p.end(),out,T{0});
  (void)remove_copy_if<all_types>(p.begin(),p.end(),out,pred);
  (void)is_partitioned<all_types>(p.begin(),p.begin(),pred);
  (void)partition_copy<all_types>(p.begin(),p.begin(),out,out,pred);
#endif

  (void)for_each<all_types>(p.begin(),p.end(),pred);
  (void)for_each_n<all_types>(p.begin(),0,pred);
     /* find: no easy way to check value_type is not compared against */
  (void)find_if<all_types>(p.begin(),p.end(),pred);
  (void)find_if_not<all_types>(p.begin(),p.end(),pred);
  (void)find_end<all_types>
    (p.begin(),p.end(),std::begin(seq),std::end(seq),pred);
  (void)find_first_of<all_types>
    (p.begin(),p.end(),std::begin(seq),std::end(seq),pred);
  (void)adjacent_find<all_types>(p.begin(),p.end(),pred);
     /* count, no easy way to check value_type is not compared against */
  (void)mismatch<all_types>(p.begin(),p.end(),std::begin(seq),pred);
  (void)equal<all_types>(p.begin(),p.end(),std::begin(seq),pred);
  (void)search<all_types>(
    p.begin(),p.end(),std::begin(seq),std::end(seq),pred);
  (void)search_n<all_types>(p.begin(),p.end(),1,T{0},pred);
  (void)copy_n<all_types>(p.begin(),0,out);
  (void)replace_copy<all_types>(p.begin(),p.end(),out,T{0},T{0});
  (void)replace_copy_if<all_types>(p.begin(),p.end(),out,pred,T{0});
  (void)unique_copy<all_types>(p.begin(),p.end(),out,pred);
  (void)sample<all_types>(p.begin(),p.begin(),out,0,std::mt19937{});
  (void)partition_point<all_types>(p.begin(),p.begin(),pred);
};

void test_algorithm4()
{
  test_algorithm<
    variant_types::collection,jammed_auto_increment,variant_types::to_int,
    variant_types::t1,variant_types::t2,variant_types::t3,
    variant_types::t4,variant_types::t5>();
  test_total_restitution_algorithm<
    variant_types::collection,variant_types::t1>();
}

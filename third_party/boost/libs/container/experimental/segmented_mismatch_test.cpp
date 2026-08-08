//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_mismatch.hpp>
#include <boost/core/lightweight_test.hpp>
#include "segmented_test_helper.hpp"
#include <boost/container/vector.hpp>
#include <utility>

using namespace boost::container;

void test_mismatch_matching()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);
   sv.add_segment_range(a3, a3 + 4);

   int ref[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
   typedef test_detail::seg_vector<int>::iterator seg_it;
   std::pair<seg_it, int*> r = segmented_mismatch(sv.begin(), sv.end(), ref);
   BOOST_TEST(r.first == sv.end());
   BOOST_TEST(r.second == ref + 9);
}

void test_mismatch_found()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);

   int ref[] = {1, 2, 3, 4, 99};
   typedef test_detail::seg_vector<int>::iterator seg_it;
   std::pair<seg_it, int*> r = segmented_mismatch(sv.begin(), sv.end(), ref);
   BOOST_TEST(r.first != sv.end());
   BOOST_TEST_EQ(*r.first, 5);
   BOOST_TEST_EQ(*r.second, 99);
}

void test_mismatch_first_segment()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);

   int ref[] = {1, 99, 3, 4, 5};
   typedef test_detail::seg_vector<int>::iterator seg_it;
   std::pair<seg_it, int*> r = segmented_mismatch(sv.begin(), sv.end(), ref);
   BOOST_TEST(r.first != sv.end());
   BOOST_TEST_EQ(*r.first, 2);
   BOOST_TEST_EQ(*r.second, 99);
}

void test_mismatch_empty()
{
   test_detail::seg_vector<int> sv;
   int dummy = 0;
   typedef test_detail::seg_vector<int>::iterator seg_it;
   std::pair<seg_it, int*> r = segmented_mismatch(sv.begin(), sv.end(), &dummy);
   BOOST_TEST(r.first == sv.end());
   BOOST_TEST(r.second == &dummy);
}

void test_mismatch_single_segment()
{
   test_detail::seg_vector<int> sv;
   int a[] = {10, 20, 30};
   sv.add_segment_range(a, a + 3);

   int ref_match[] = {10, 20, 30};
   typedef test_detail::seg_vector<int>::iterator seg_it;
   std::pair<seg_it, int*> r = segmented_mismatch(sv.begin(), sv.end(), ref_match);
   BOOST_TEST(r.first == sv.end());

   int ref_fail[] = {10, 20, 99};
   r = segmented_mismatch(sv.begin(), sv.end(), ref_fail);
   BOOST_TEST(r.first != sv.end());
   BOOST_TEST_EQ(*r.first, 30);
   BOOST_TEST_EQ(*r.second, 99);
}

void test_mismatch_non_segmented()
{
   boost::container::vector<int> v;
   v.push_back(1); v.push_back(2); v.push_back(3);

   int ref_match[] = {1, 2, 3};
   typedef boost::container::vector<int>::iterator vec_it;
   std::pair<vec_it, int*> r = segmented_mismatch(v.begin(), v.end(), ref_match);
   BOOST_TEST(r.first == v.end());

   int ref_fail[] = {1, 2, 99};
   r = segmented_mismatch(v.begin(), v.end(), ref_fail);
   BOOST_TEST(r.first != v.end());
   BOOST_TEST_EQ(*r.first, 3);
   BOOST_TEST_EQ(*r.second, 99);
}

void test_mismatch_sentinel_segmented()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);
   sv.add_segment_range(a3, a3 + 4);

   int ref[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
   typedef test_detail::seg_vector<int>::iterator seg_it;
   std::pair<seg_it, int*> r =
      segmented_mismatch(sv.begin(), test_detail::make_sentinel(sv.end()), ref);
   BOOST_TEST(r.first == sv.end());
   BOOST_TEST(r.second == ref + 9);
}

void test_mismatch_sentinel_non_segmented()
{
   boost::container::vector<int> v;
   v.push_back(1); v.push_back(2); v.push_back(3);

   int ref_match[] = {1, 2, 3};
   typedef boost::container::vector<int>::iterator vec_it;
   std::pair<vec_it, int*> r =
      segmented_mismatch(v.begin(), test_detail::make_sentinel(v.end()), ref_match);
   BOOST_TEST(r.first == v.end());

   int ref_fail[] = {1, 2, 99};
   r = segmented_mismatch(v.begin(), test_detail::make_sentinel(v.end()), ref_fail);
   BOOST_TEST(r.first != v.end());
   BOOST_TEST_EQ(*r.first, 3);
   BOOST_TEST_EQ(*r.second, 99);
}

void test_mismatch_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 2);
   sv2.add_flat_segment_range(a3, a3 + 4);

   int ref[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
   typedef test_detail::seg2_vector<int>::iterator seg2_it;
   std::pair<seg2_it, int*> r = segmented_mismatch(sv2.begin(), sv2.end(), ref);
   BOOST_TEST(r.first == sv2.end());

   int ref_bad[] = {1, 2, 3, 4, 5, 6, 7, 8, 0};
   r = segmented_mismatch(sv2.begin(), sv2.end(), ref_bad);
   BOOST_TEST(r.first != sv2.end());
   BOOST_TEST_EQ(*r.first, 9);
   BOOST_TEST_EQ(*r.second, 0);
}

void test_mismatch_every_position()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {10, 20, 30};
   int a2[] = {40, 50};
   int a3[] = {60, 70, 80, 90};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);
   sv.add_segment_range(a3, a3 + 4);

   int vals[] = {10, 20, 30, 40, 50, 60, 70, 80, 90};
   const int N = 9;
   typedef test_detail::seg_vector<int>::iterator iter_t;

   for(int pos = 0; pos < N; ++pos) {
      int ref[9];
      for(int j = 0; j < N; ++j) ref[j] = vals[j];
      ref[pos] = -1;

      iter_t expected = sv.begin();
      for(int j = 0; j < pos; ++j) ++expected;

      std::pair<iter_t, int*> r = segmented_mismatch(sv.begin(), sv.end(), ref);
      BOOST_TEST(r.first == expected);
      BOOST_TEST_EQ(*r.first, vals[pos]);
      BOOST_TEST_EQ(*r.second, -1);
   }

   std::pair<iter_t, int*> r = segmented_mismatch(sv.begin(), sv.end(), vals);
   BOOST_TEST(r.first == sv.end());
}

void test_mismatch_every_position_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {10, 20, 30};
   int a2[] = {40, 50};
   int a3[] = {60, 70, 80, 90};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 2);
   sv2.add_flat_segment_range(a3, a3 + 4);

   int vals[] = {10, 20, 30, 40, 50, 60, 70, 80, 90};
   const int N = 9;
   typedef test_detail::seg2_vector<int>::iterator iter_t;

   for(int pos = 0; pos < N; ++pos) {
      int ref[9];
      for(int j = 0; j < N; ++j) ref[j] = vals[j];
      ref[pos] = -1;

      iter_t expected = sv2.begin();
      for(int j = 0; j < pos; ++j) ++expected;

      std::pair<iter_t, int*> r = segmented_mismatch(sv2.begin(), sv2.end(), ref);
      BOOST_TEST(r.first == expected);
      BOOST_TEST_EQ(*r.first, vals[pos]);
      BOOST_TEST_EQ(*r.second, -1);
   }

   std::pair<iter_t, int*> r = segmented_mismatch(sv2.begin(), sv2.end(), vals);
   BOOST_TEST(r.first == sv2.end());
}

void test_mismatch_seg_to_seg()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);
   sv.add_segment_range(a3, a3 + 4);

   test_detail::seg_vector<int> sv2;
   int b1[] = {1, 2, 3, 4};
   int b2[] = {5, 6, 7, 8};
   int b3[] = {9};
   sv2.add_segment_range(b1, b1 + 4);
   sv2.add_segment_range(b2, b2 + 4);
   sv2.add_segment_range(b3, b3 + 1);

   typedef test_detail::seg_vector<int>::iterator iter_t;
   std::pair<iter_t, iter_t> r = segmented_mismatch(sv.begin(), sv.end(), sv2.begin());
   BOOST_TEST(r.first == sv.end());
}

void test_mismatch_seg_to_seg_mismatch()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);
   sv.add_segment_range(a3, a3 + 4);

   test_detail::seg_vector<int> sv2;
   int b1[] = {1, 2, 3, 4};
   int b2[] = {5, 6, 99, 8};
   int b3[] = {9};
   sv2.add_segment_range(b1, b1 + 4);
   sv2.add_segment_range(b2, b2 + 4);
   sv2.add_segment_range(b3, b3 + 1);

   typedef test_detail::seg_vector<int>::iterator iter_t;
   std::pair<iter_t, iter_t> r = segmented_mismatch(sv.begin(), sv.end(), sv2.begin());
   BOOST_TEST(r.first != sv.end());
   BOOST_TEST_EQ(*r.first, 7);
   BOOST_TEST_EQ(*r.second, 99);
}

void test_mismatch_seg2_to_seg2()
{
   test_detail::seg2_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv.add_flat_segment_range(a1, a1 + 3);
   sv.add_flat_segment_range(a2, a2 + 2);
   sv.add_flat_segment_range(a3, a3 + 4);

   test_detail::seg2_vector<int> sv2;
   int b1[] = {1, 2, 3, 4, 5};
   int b2[] = {6, 7, 8, 9};
   sv2.add_flat_segment_range(b1, b1 + 5);
   sv2.add_flat_segment_range(b2, b2 + 4);

   typedef test_detail::seg2_vector<int>::iterator iter_t;
   std::pair<iter_t, iter_t> r = segmented_mismatch(sv.begin(), sv.end(), sv2.begin());
   BOOST_TEST(r.first == sv.end());
}

void test_mismatch_seg_to_seg_misaligned()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3, 4, 5};
   int a2[] = {6, 7, 8};
   sv.add_segment_range(a1, a1 + 5);
   sv.add_segment_range(a2, a2 + 3);

   test_detail::seg_vector<int> sv2;
   int b1[] = {1, 2};
   int b2[] = {3, 4, 5};
   int b3[] = {6, 7, 8};
   sv2.add_segment_range(b1, b1 + 2);
   sv2.add_segment_range(b2, b2 + 3);
   sv2.add_segment_range(b3, b3 + 3);

   typedef test_detail::seg_vector<int>::iterator iter_t;
   std::pair<iter_t, iter_t> r = segmented_mismatch(sv.begin(), sv.end(), sv2.begin());
   BOOST_TEST(r.first == sv.end());
}

void test_mismatch_seg_to_seg_every_position()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {10, 20, 30};
   int a2[] = {40, 50};
   int a3[] = {60, 70, 80, 90};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);
   sv.add_segment_range(a3, a3 + 4);

   int vals[] = {10, 20, 30, 40, 50, 60, 70, 80, 90};
   const int N = 9;
   typedef test_detail::seg_vector<int>::iterator iter_t;

   for(int pos = 0; pos < N; ++pos) {
      test_detail::seg_vector<int> sv2;
      int ref[9];
      for(int j = 0; j < N; ++j) ref[j] = vals[j];
      ref[pos] = -1;
      int r1[] = {ref[0], ref[1], ref[2], ref[3]};
      int r2[] = {ref[4], ref[5], ref[6], ref[7]};
      int r3[] = {ref[8]};
      sv2.add_segment_range(r1, r1 + 4);
      sv2.add_segment_range(r2, r2 + 4);
      sv2.add_segment_range(r3, r3 + 1);

      iter_t expected = sv.begin();
      for(int j = 0; j < pos; ++j) ++expected;

      std::pair<iter_t, iter_t> r = segmented_mismatch(sv.begin(), sv.end(), sv2.begin());
      BOOST_TEST(r.first == expected);
      BOOST_TEST_EQ(*r.first, vals[pos]);
      BOOST_TEST_EQ(*r.second, -1);
   }
}

//////////////////////////////////////////////////////////////////////////////
// Tests for the two-range overloads (first1, last1, first2, last2[, pred])
//////////////////////////////////////////////////////////////////////////////

void test_mismatch_2r_equal_same_length()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);
   sv.add_segment_range(a3, a3 + 4);

   int ref[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
   typedef test_detail::seg_vector<int>::iterator seg_it;
   std::pair<seg_it, int*> r = segmented_mismatch(sv.begin(), sv.end(), ref, ref + 9);
   BOOST_TEST(r.first  == sv.end());
   BOOST_TEST(r.second == ref + 9);
}

void test_mismatch_2r_second_shorter()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);

   int ref[] = {1, 2, 3, 4};
   typedef test_detail::seg_vector<int>::iterator seg_it;
   std::pair<seg_it, int*> r = segmented_mismatch(sv.begin(), sv.end(), ref, ref + 4);
   BOOST_TEST(r.second == ref + 4);
   BOOST_TEST(r.first  != sv.end());
   BOOST_TEST_EQ(*r.first, 5);
}

void test_mismatch_2r_first_shorter()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2};
   int a2[] = {3};
   sv.add_segment_range(a1, a1 + 2);
   sv.add_segment_range(a2, a2 + 1);

   int ref[] = {1, 2, 3, 4, 5};
   typedef test_detail::seg_vector<int>::iterator seg_it;
   std::pair<seg_it, int*> r = segmented_mismatch(sv.begin(), sv.end(), ref, ref + 5);
   BOOST_TEST(r.first  == sv.end());
   BOOST_TEST_EQ(*r.second, 4);
}

void test_mismatch_2r_mismatch_within_common()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);

   int ref[] = {1, 2, 99, 4, 5, 6};
   typedef test_detail::seg_vector<int>::iterator seg_it;
   std::pair<seg_it, int*> r = segmented_mismatch(sv.begin(), sv.end(), ref, ref + 6);
   BOOST_TEST_EQ(*r.first,  3);
   BOOST_TEST_EQ(*r.second, 99);
}

void test_mismatch_2r_both_empty()
{
   test_detail::seg_vector<int> sv;
   int dummy = 0;
   typedef test_detail::seg_vector<int>::iterator seg_it;
   std::pair<seg_it, int*> r = segmented_mismatch(sv.begin(), sv.end(), &dummy, &dummy);
   BOOST_TEST(r.first  == sv.end());
   BOOST_TEST(r.second == &dummy);
}

void test_mismatch_2r_first_empty()
{
   test_detail::seg_vector<int> sv;
   int ref[] = {1, 2, 3};
   typedef test_detail::seg_vector<int>::iterator seg_it;
   std::pair<seg_it, int*> r = segmented_mismatch(sv.begin(), sv.end(), ref, ref + 3);
   BOOST_TEST(r.first  == sv.end());
   BOOST_TEST(r.second == ref);
}

void test_mismatch_2r_second_empty()
{
   test_detail::seg_vector<int> sv;
   int a[] = {1, 2, 3};
   sv.add_segment_range(a, a + 3);

   int dummy = 0;
   typedef test_detail::seg_vector<int>::iterator seg_it;
   std::pair<seg_it, int*> r = segmented_mismatch(sv.begin(), sv.end(), &dummy, &dummy);
   BOOST_TEST(r.first  == sv.begin());
   BOOST_TEST(r.second == &dummy);
}

struct test_mismatch_double_eq {
   bool operator()(int a, int b) const { return a * 2 == b; }
};

void test_mismatch_2r_with_pred()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);

   int ref[] = {2, 4, 6, 8, 10};
   typedef test_mismatch_double_eq double_eq;
   typedef test_detail::seg_vector<int>::iterator seg_it;
   std::pair<seg_it, int*> r =
      segmented_mismatch(sv.begin(), sv.end(), ref + 0, ref + 5, double_eq());
   BOOST_TEST(r.first  == sv.end());
   BOOST_TEST(r.second == ref + 5);

   int ref_bad[] = {2, 4, 99, 8, 10};
   r = segmented_mismatch(sv.begin(), sv.end(), ref_bad + 0, ref_bad + 5, double_eq());
   BOOST_TEST_EQ(*r.first,  3);
   BOOST_TEST_EQ(*r.second, 99);
}

void test_mismatch_2r_non_segmented()
{
   boost::container::vector<int> v;
   v.push_back(1); v.push_back(2); v.push_back(3);

   int ref_match[] = {1, 2, 3, 4};
   typedef boost::container::vector<int>::iterator vec_it;
   std::pair<vec_it, int*> r = segmented_mismatch(v.begin(), v.end(), ref_match, ref_match + 4);
   BOOST_TEST(r.first  == v.end());
   BOOST_TEST_EQ(*r.second, 4);

   int ref_fail[] = {1, 99};
   r = segmented_mismatch(v.begin(), v.end(), ref_fail, ref_fail + 2);
   BOOST_TEST_EQ(*r.first,  2);
   BOOST_TEST_EQ(*r.second, 99);
}

void test_mismatch_2r_sentinel_first_range()
{
   // Sentinel for last1, regular iterator for last2 via the 5-arg pred form.
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);
   sv.add_segment_range(a3, a3 + 4);

   int ref[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
   typedef test_detail::seg_vector<int>::iterator seg_it;
   std::pair<seg_it, int*> r = segmented_mismatch
      (sv.begin(), test_detail::make_sentinel(sv.end()),
       ref, ref + 9,
       boost::container::detail_algo::mismatch_equal());
   BOOST_TEST(r.first  == sv.end());
   BOOST_TEST(r.second == ref + 9);
}

void test_mismatch_2r_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 2);
   sv2.add_flat_segment_range(a3, a3 + 4);

   int ref[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
   typedef test_detail::seg2_vector<int>::iterator seg2_it;
   std::pair<seg2_it, int*> r = segmented_mismatch(sv2.begin(), sv2.end(), ref, ref + 9);
   BOOST_TEST(r.first  == sv2.end());
   BOOST_TEST(r.second == ref + 9);

   int ref_bad[] = {1, 2, 3, 4, 5, 6, 7, 0};
   r = segmented_mismatch(sv2.begin(), sv2.end(), ref_bad, ref_bad + 8);
   BOOST_TEST_EQ(*r.first,  8);
   BOOST_TEST_EQ(*r.second, 0);
}

void test_mismatch_2r_seg_to_seg_equal()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);
   sv.add_segment_range(a3, a3 + 4);

   test_detail::seg_vector<int> sv2;
   int b1[] = {1, 2, 3, 4};
   int b2[] = {5, 6, 7, 8};
   int b3[] = {9};
   sv2.add_segment_range(b1, b1 + 4);
   sv2.add_segment_range(b2, b2 + 4);
   sv2.add_segment_range(b3, b3 + 1);

   typedef test_detail::seg_vector<int>::iterator iter_t;
   std::pair<iter_t, iter_t> r = segmented_mismatch(sv.begin(), sv.end(), sv2.begin(), sv2.end());
   BOOST_TEST(r.first  == sv.end());
   BOOST_TEST(r.second == sv2.end());
}

void test_mismatch_2r_seg_to_seg_second_shorter()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);

   test_detail::seg_vector<int> sv2;
   int b1[] = {1, 2};
   int b2[] = {3, 4};
   sv2.add_segment_range(b1, b1 + 2);
   sv2.add_segment_range(b2, b2 + 2);

   typedef test_detail::seg_vector<int>::iterator iter_t;
   std::pair<iter_t, iter_t> r = segmented_mismatch(sv.begin(), sv.end(), sv2.begin(), sv2.end());
   BOOST_TEST(r.second == sv2.end());
   BOOST_TEST(r.first  != sv.end());
   BOOST_TEST_EQ(*r.first, 5);
}

void test_mismatch_2r_seg_to_seg_mismatch_straddle()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);
   sv.add_segment_range(a3, a3 + 4);

   test_detail::seg_vector<int> sv2;
   int b1[] = {1, 2, 3, 4};
   int b2[] = {5, 6, 99, 8};
   int b3[] = {9};
   sv2.add_segment_range(b1, b1 + 4);
   sv2.add_segment_range(b2, b2 + 4);
   sv2.add_segment_range(b3, b3 + 1);

   typedef test_detail::seg_vector<int>::iterator iter_t;
   std::pair<iter_t, iter_t> r = segmented_mismatch(sv.begin(), sv.end(), sv2.begin(), sv2.end());
   BOOST_TEST(r.first  != sv.end());
   BOOST_TEST_EQ(*r.first,  7);
   BOOST_TEST_EQ(*r.second, 99);
}

void test_mismatch_2r_seg2_to_seg2()
{
   test_detail::seg2_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv.add_flat_segment_range(a1, a1 + 3);
   sv.add_flat_segment_range(a2, a2 + 2);
   sv.add_flat_segment_range(a3, a3 + 4);

   test_detail::seg2_vector<int> sv2;
   int b1[] = {1, 2, 3, 4, 5};
   int b2[] = {6, 7, 8, 9};
   sv2.add_flat_segment_range(b1, b1 + 5);
   sv2.add_flat_segment_range(b2, b2 + 4);

   typedef test_detail::seg2_vector<int>::iterator iter_t;
   std::pair<iter_t, iter_t> r = segmented_mismatch(sv.begin(), sv.end(), sv2.begin(), sv2.end());
   BOOST_TEST(r.first  == sv.end());
   BOOST_TEST(r.second == sv2.end());
}

void test_mismatch_2r_every_position()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {10, 20, 30};
   int a2[] = {40, 50};
   int a3[] = {60, 70, 80, 90};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);
   sv.add_segment_range(a3, a3 + 4);

   int vals[] = {10, 20, 30, 40, 50, 60, 70, 80, 90};
   const int N = 9;
   typedef test_detail::seg_vector<int>::iterator iter_t;

   for(int pos = 0; pos < N; ++pos) {
      int ref[9];
      for(int j = 0; j < N; ++j) ref[j] = vals[j];
      ref[pos] = -1;

      iter_t expected = sv.begin();
      for(int j = 0; j < pos; ++j) ++expected;

      std::pair<iter_t, int*> r = segmented_mismatch(sv.begin(), sv.end(), ref, ref + N);
      BOOST_TEST(r.first == expected);
      BOOST_TEST_EQ(*r.first,  vals[pos]);
      BOOST_TEST_EQ(*r.second, -1);
   }
}

void test_mismatch_2r_seg_to_seg_every_position()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {10, 20, 30};
   int a2[] = {40, 50};
   int a3[] = {60, 70, 80, 90};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);
   sv.add_segment_range(a3, a3 + 4);

   int vals[] = {10, 20, 30, 40, 50, 60, 70, 80, 90};
   const int N = 9;
   typedef test_detail::seg_vector<int>::iterator iter_t;

   for(int pos = 0; pos < N; ++pos) {
      test_detail::seg_vector<int> sv2;
      int ref[9];
      for(int j = 0; j < N; ++j) ref[j] = vals[j];
      ref[pos] = -1;
      int r1[] = {ref[0], ref[1], ref[2], ref[3]};
      int r2[] = {ref[4], ref[5], ref[6], ref[7]};
      int r3[] = {ref[8]};
      sv2.add_segment_range(r1, r1 + 4);
      sv2.add_segment_range(r2, r2 + 4);
      sv2.add_segment_range(r3, r3 + 1);

      iter_t expected = sv.begin();
      for(int j = 0; j < pos; ++j) ++expected;

      std::pair<iter_t, iter_t> r = segmented_mismatch(sv.begin(), sv.end(), sv2.begin(), sv2.end());
      BOOST_TEST(r.first == expected);
      BOOST_TEST_EQ(*r.first,  vals[pos]);
      BOOST_TEST_EQ(*r.second, -1);
   }
}

int main()
{
   test_mismatch_matching();
   test_mismatch_found();
   test_mismatch_first_segment();
   test_mismatch_empty();
   test_mismatch_single_segment();
   test_mismatch_non_segmented();
   test_mismatch_sentinel_segmented();
   test_mismatch_sentinel_non_segmented();
   test_mismatch_seg2();
   test_mismatch_every_position();
   test_mismatch_every_position_seg2();
   test_mismatch_seg_to_seg();
   test_mismatch_seg_to_seg_mismatch();
   test_mismatch_seg2_to_seg2();
   test_mismatch_seg_to_seg_misaligned();
   test_mismatch_seg_to_seg_every_position();

   // Two-range (bounded on both sides) overloads:
   test_mismatch_2r_equal_same_length();
   test_mismatch_2r_second_shorter();
   test_mismatch_2r_first_shorter();
   test_mismatch_2r_mismatch_within_common();
   test_mismatch_2r_both_empty();
   test_mismatch_2r_first_empty();
   test_mismatch_2r_second_empty();
   test_mismatch_2r_with_pred();
   test_mismatch_2r_non_segmented();
   test_mismatch_2r_sentinel_first_range();
   test_mismatch_2r_seg2();
   test_mismatch_2r_seg_to_seg_equal();
   test_mismatch_2r_seg_to_seg_second_shorter();
   test_mismatch_2r_seg_to_seg_mismatch_straddle();
   test_mismatch_2r_seg2_to_seg2();
   test_mismatch_2r_every_position();
   test_mismatch_2r_seg_to_seg_every_position();

   return boost::report_errors();
}

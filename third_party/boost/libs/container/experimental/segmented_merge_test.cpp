//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_merge.hpp>
#include <boost/core/lightweight_test.hpp>
#include "segmented_test_helper.hpp"
#include <boost/container/vector.hpp>

using namespace boost::container;

void test_merge_segmented_inputs()
{
   test_detail::seg_vector<int> sv1;
   int a1[] = {1, 3};
   int a2[] = {5, 7};
   sv1.add_segment_range(a1, a1 + 2);
   sv1.add_segment_range(a2, a2 + 2);

   test_detail::seg_vector<int> sv2;
   int b1[] = {2, 4};
   int b2[] = {6, 8};
   sv2.add_segment_range(b1, b1 + 2);
   sv2.add_segment_range(b2, b2 + 2);

   boost::container::vector<int> out(8, 0);
   boost::container::vector<int>::iterator r = segmented_merge(
      sv1.begin(), sv1.end(), sv2.begin(), sv2.end(), out.begin());

   BOOST_TEST_EQ(static_cast<std::size_t>(r - out.begin()), 8u);
   for(int i = 0; i < 8; ++i)
      BOOST_TEST_EQ(out[static_cast<std::size_t>(i)], i + 1);
}

void test_merge_first_empty()
{
   test_detail::seg_vector<int> sv1;
   test_detail::seg_vector<int> sv2;
   int a[] = {1, 2, 3};
   sv2.add_segment_range(a, a + 3);

   boost::container::vector<int> out(3, 0);
   boost::container::vector<int>::iterator r = segmented_merge(
      sv1.begin(), sv1.end(), sv2.begin(), sv2.end(), out.begin());

   BOOST_TEST_EQ(static_cast<std::size_t>(r - out.begin()), 3u);
   BOOST_TEST_EQ(out[0], 1);
   BOOST_TEST_EQ(out[1], 2);
   BOOST_TEST_EQ(out[2], 3);
}

void test_merge_second_empty()
{
   test_detail::seg_vector<int> sv1;
   int a[] = {1, 2, 3};
   sv1.add_segment_range(a, a + 3);
   test_detail::seg_vector<int> sv2;

   boost::container::vector<int> out(3, 0);
   boost::container::vector<int>::iterator r = segmented_merge(
      sv1.begin(), sv1.end(), sv2.begin(), sv2.end(), out.begin());

   BOOST_TEST_EQ(static_cast<std::size_t>(r - out.begin()), 3u);
   BOOST_TEST_EQ(out[0], 1);
   BOOST_TEST_EQ(out[1], 2);
   BOOST_TEST_EQ(out[2], 3);
}

void test_merge_both_empty()
{
   test_detail::seg_vector<int> sv1;
   test_detail::seg_vector<int> sv2;
   boost::container::vector<int> out;
   boost::container::vector<int>::iterator r = segmented_merge(
      sv1.begin(), sv1.end(), sv2.begin(), sv2.end(), out.begin());
   BOOST_TEST(r == out.begin());
}

struct greater_comp
{
   bool operator()(int a, int b) const { return a > b; }
};

void test_merge_with_comp()
{
   int a[] = {5, 3, 1};
   int b[] = {6, 4, 2};
   boost::container::vector<int> v1(a, a + 3);
   boost::container::vector<int> v2(b, b + 3);
   boost::container::vector<int> out(6, 0);

   boost::container::vector<int>::iterator r = segmented_merge(
      v1.begin(), v1.end(), v2.begin(), v2.end(), out.begin(), greater_comp());

   BOOST_TEST_EQ(static_cast<std::size_t>(r - out.begin()), 6u);
   BOOST_TEST_EQ(out[0], 6);
   BOOST_TEST_EQ(out[1], 5);
   BOOST_TEST_EQ(out[2], 4);
   BOOST_TEST_EQ(out[3], 3);
   BOOST_TEST_EQ(out[4], 2);
   BOOST_TEST_EQ(out[5], 1);
}

void test_merge_non_segmented()
{
   int a[] = {1, 3, 5};
   int b[] = {2, 4, 6};
   boost::container::vector<int> v1(a, a + 3);
   boost::container::vector<int> v2(b, b + 3);
   boost::container::vector<int> out(6, 0);

   boost::container::vector<int>::iterator r = segmented_merge(
      v1.begin(), v1.end(), v2.begin(), v2.end(), out.begin());

   BOOST_TEST_EQ(static_cast<std::size_t>(r - out.begin()), 6u);
   for(int i = 0; i < 6; ++i)
      BOOST_TEST_EQ(out[static_cast<std::size_t>(i)], i + 1);
}

void test_merge_duplicates()
{
   int a[] = {1, 2, 3};
   int b[] = {2, 3, 4};
   boost::container::vector<int> v1(a, a + 3);
   boost::container::vector<int> v2(b, b + 3);
   boost::container::vector<int> out(6, 0);

   boost::container::vector<int>::iterator r = segmented_merge(
      v1.begin(), v1.end(), v2.begin(), v2.end(), out.begin());

   BOOST_TEST_EQ(static_cast<std::size_t>(r - out.begin()), 6u);
   int expected[] = {1, 2, 2, 3, 3, 4};
   for(std::size_t i = 0; i < 6; ++i)
      BOOST_TEST_EQ(out[i], expected[i]);
}

void test_merge_sentinel_segmented()
{
   test_detail::seg_vector<int> sv1;
   int a1[] = {1, 3};
   int a2[] = {5, 7};
   sv1.add_segment_range(a1, a1 + 2);
   sv1.add_segment_range(a2, a2 + 2);

   test_detail::seg_vector<int> sv2;
   int b1[] = {2, 4};
   int b2[] = {6, 8};
   sv2.add_segment_range(b1, b1 + 2);
   sv2.add_segment_range(b2, b2 + 2);

   boost::container::vector<int> out(8, 0);
   boost::container::vector<int>::iterator r = segmented_merge(
      sv1.begin(), test_detail::make_sentinel(sv1.end()),
      sv2.begin(), test_detail::make_sentinel(sv2.end()), out.begin());

   BOOST_TEST_EQ(static_cast<std::size_t>(r - out.begin()), 8u);
   for(int i = 0; i < 8; ++i)
      BOOST_TEST_EQ(out[static_cast<std::size_t>(i)], i + 1);
}

void test_merge_sentinel_non_segmented()
{
   int a[] = {1, 3, 5};
   int b[] = {2, 4, 6};
   boost::container::vector<int> v1(a, a + 3);
   boost::container::vector<int> v2(b, b + 3);
   boost::container::vector<int> out(6, 0);

   boost::container::vector<int>::iterator r = segmented_merge(
      v1.begin(), test_detail::make_sentinel(v1.end()),
      v2.begin(), test_detail::make_sentinel(v2.end()), out.begin());

   BOOST_TEST_EQ(static_cast<std::size_t>(r - out.begin()), 6u);
   for(int i = 0; i < 6; ++i)
      BOOST_TEST_EQ(out[static_cast<std::size_t>(i)], i + 1);
}

void test_merge_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {1, 3, 5};
   int a2[] = {7, 9};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 2);

   int b[] = {2, 4, 6, 8};
   boost::container::vector<int> out(9, 0);
   boost::container::vector<int>::iterator r = segmented_merge(
      sv2.begin(), sv2.end(), b, b + 4, out.begin());

   BOOST_TEST_EQ(static_cast<std::size_t>(r - out.begin()), 9u);
   for(int i = 0; i < 9; ++i)
      BOOST_TEST_EQ(out[static_cast<std::size_t>(i)], i + 1);
}

// Multi-segmented output with non-segmented inputs.  Exercises the
// merge_dst_dispatch(segmented_iterator_tag) walker that walks output
// segments and bounds each merge call to the current segment.
void test_merge_segmented_output()
{
   int a[] = {1, 3, 5, 7, 9};
   int b[] = {2, 4, 6, 8};

   test_detail::seg_vector<int> out;
   out.add_segment(3, 0);
   out.add_segment(4, 0);
   out.add_segment(2, 0);

   typedef test_detail::seg_vector<int>::iterator iter_t;
   iter_t r = segmented_merge(a, a + 5, b, b + 4, out.begin());

   BOOST_TEST(r == out.end());
   iter_t it = out.begin();
   for(int i = 0; i < 9; ++i, ++it)
      BOOST_TEST_EQ(*it, i + 1);
}

// Segmented inputs and segmented output: stresses the per-src1-segment
// walker threading first2/result through the multi-segmented dst walker.
void test_merge_segmented_inputs_and_output()
{
   test_detail::seg_vector<int> sv1;
   int a1[] = {1, 3};
   int a2[] = {5, 7, 9};
   sv1.add_segment_range(a1, a1 + 2);
   sv1.add_segment_range(a2, a2 + 3);

   test_detail::seg_vector<int> sv2;
   int b1[] = {2, 4};
   int b2[] = {6, 8};
   sv2.add_segment_range(b1, b1 + 2);
   sv2.add_segment_range(b2, b2 + 2);

   test_detail::seg_vector<int> out;
   out.add_segment(2, 0);
   out.add_segment(3, 0);
   out.add_segment(4, 0);

   typedef test_detail::seg_vector<int>::iterator iter_t;
   iter_t r = segmented_merge(
      sv1.begin(), sv1.end(), sv2.begin(), sv2.end(), out.begin());

   BOOST_TEST(r == out.end());
   iter_t it = out.begin();
   for(int i = 0; i < 9; ++i, ++it)
      BOOST_TEST_EQ(*it, i + 1);
}

// First source larger than second: forces the first2-exhausted branch
// of merge_dst_dispatch(segmented_iterator_tag) which drains the
// remaining first1 via segmented_copy through the rest of dst.
void test_merge_segmented_output_first_longer()
{
   int a[] = {1, 3, 5, 7, 9, 11, 13};
   int b[] = {2, 4};

   test_detail::seg_vector<int> out;
   out.add_segment(2, 0);
   out.add_segment(2, 0);
   out.add_segment(2, 0);
   out.add_segment(3, 0);

   typedef test_detail::seg_vector<int>::iterator iter_t;
   iter_t r = segmented_merge(a, a + 7, b, b + 2, out.begin());

   BOOST_TEST(r == out.end());
   int expected[] = {1, 2, 3, 4, 5, 7, 9, 11, 13};
   iter_t it = out.begin();
   for(std::size_t i = 0; i < 9; ++i, ++it)
      BOOST_TEST_EQ(*it, expected[i]);
}

// Second source larger than first: forces the first1-exhausted branch
// where the leaf returns and then the remaining first2 is flushed via
// segmented_copy at the top-level segmented_merge_dispatch.
void test_merge_segmented_output_second_longer()
{
   int a[] = {1, 3};
   int b[] = {2, 4, 6, 8, 10, 12, 14};

   test_detail::seg_vector<int> out;
   out.add_segment(3, 0);
   out.add_segment(3, 0);
   out.add_segment(3, 0);

   typedef test_detail::seg_vector<int>::iterator iter_t;
   iter_t r = segmented_merge(a, a + 2, b, b + 7, out.begin());

   BOOST_TEST(r == out.end());
   int expected[] = {1, 2, 3, 4, 6, 8, 10, 12, 14};
   iter_t it = out.begin();
   for(std::size_t i = 0; i < 9; ++i, ++it)
      BOOST_TEST_EQ(*it, expected[i]);
}

// Stress test for the unrolled main loop.  With N=64 in both ranges, the
// unrolled fast path of merge_dst_bounded executes at least 16 times and
// the count-based tail handles the remainder.  Exercises the various
// dst-flavour code paths (unbounded, bounded RA, segmented).
void test_merge_stress_long_ranges()
{
   const int N = 64;
   boost::container::vector<int> v1, v2;
   v1.reserve(N); v2.reserve(N);
   for(int i = 0; i < N; ++i) v1.push_back(2 * i);          // 0, 2, 4, ...
   for(int i = 0; i < N; ++i) v2.push_back(2 * i + 1);      // 1, 3, 5, ...

   // Flat dst (RA, unbounded path through dual-RA when sized exactly).
   boost::container::vector<int> out_flat(2 * N, -1);
   boost::container::vector<int>::iterator r1 = segmented_merge(
      v1.begin(), v1.end(), v2.begin(), v2.end(), out_flat.begin());
   BOOST_TEST_EQ(static_cast<std::size_t>(r1 - out_flat.begin()),
                 static_cast<std::size_t>(2 * N));
   for(int i = 0; i < 2 * N; ++i)
      BOOST_TEST_EQ(out_flat[static_cast<std::size_t>(i)], i);

   // Segmented dst with non-uniform segments: exercises unrolled body
   // inside the segmented walker (each per-segment call hits the
   // unrolled overload with bounded RA dst).
   test_detail::seg_vector<int> out_seg;
   out_seg.add_segment(13, 0);
   out_seg.add_segment(31, 0);
   out_seg.add_segment(7, 0);
   out_seg.add_segment(2 * N - 13 - 31 - 7, 0);

   typedef test_detail::seg_vector<int>::iterator seg_iter_t;
   seg_iter_t r2 = segmented_merge(
      v1.begin(), v1.end(), v2.begin(), v2.end(), out_seg.begin());
   BOOST_TEST(r2 == out_seg.end());
   seg_iter_t it = out_seg.begin();
   for(int i = 0; i < 2 * N; ++i, ++it)
      BOOST_TEST_EQ(*it, i);
}

// Non-segmented first1 + segmented first2 + non-segmented (RA) dst.
// Exercises the new merge_seg2_dispatch walker: each first2 segment is
// fed flat to the bounded leaf, so the dual-RA / unrolled fast paths fire
// per segment rather than degrading to the generic input-iterator merge.
void test_merge_seg2_walker_flat_dst()
{
   int a[] = {2, 4, 6, 8, 10};
   test_detail::seg_vector<int> sv2;
   int b1[] = {1, 3};
   int b2[] = {5, 7};
   int b3[] = {9, 11, 13};
   sv2.add_segment_range(b1, b1 + 2);
   sv2.add_segment_range(b2, b2 + 2);
   sv2.add_segment_range(b3, b3 + 3);

   boost::container::vector<int> out(12, 0);
   boost::container::vector<int>::iterator r = segmented_merge(
      a, a + 5, sv2.begin(), sv2.end(), out.begin());

   BOOST_TEST_EQ(static_cast<std::size_t>(r - out.begin()), 12u);
   int expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13};
   for(std::size_t i = 0; i < 12; ++i)
      BOOST_TEST_EQ(out[i], expected[i]);
}

// Non-segmented first1 + segmented first2 + segmented dst.
// Exercises the cross product of the seg2 walker with the seg-dst walker
// inside merge_until_exhausts (segmented dst overload).
void test_merge_seg2_walker_seg_dst()
{
   int a[] = {2, 4, 6, 8, 10};
   test_detail::seg_vector<int> sv2;
   int b1[] = {1, 3};
   int b2[] = {5, 7};
   int b3[] = {9, 11, 13};
   sv2.add_segment_range(b1, b1 + 2);
   sv2.add_segment_range(b2, b2 + 2);
   sv2.add_segment_range(b3, b3 + 3);

   test_detail::seg_vector<int> out;
   out.add_segment(3, 0);
   out.add_segment(2, 0);
   out.add_segment(5, 0);
   out.add_segment(2, 0);

   typedef test_detail::seg_vector<int>::iterator iter_t;
   iter_t r = segmented_merge(a, a + 5, sv2.begin(), sv2.end(), out.begin());

   BOOST_TEST(r == out.end());
   int expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13};
   iter_t it = out.begin();
   for(std::size_t i = 0; i < 12; ++i, ++it)
      BOOST_TEST_EQ(*it, expected[i]);
}

// first1 longer than first2: forces seg2 walker to fully consume all
// first2 segments and then drain the remaining first1 via segmented_copy.
void test_merge_seg2_walker_first1_longer()
{
   int a[] = {1, 3, 5, 7, 9, 11, 13, 15, 17};
   test_detail::seg_vector<int> sv2;
   int b1[] = {2};
   int b2[] = {4, 6};
   sv2.add_segment_range(b1, b1 + 1);
   sv2.add_segment_range(b2, b2 + 2);

   boost::container::vector<int> out(12, 0);
   boost::container::vector<int>::iterator r = segmented_merge(
      a, a + 9, sv2.begin(), sv2.end(), out.begin());

   BOOST_TEST_EQ(static_cast<std::size_t>(r - out.begin()), 12u);
   int expected[] = {1, 2, 3, 4, 5, 6, 7, 9, 11, 13, 15, 17};
   for(std::size_t i = 0; i < 12; ++i)
      BOOST_TEST_EQ(out[i], expected[i]);
}

// first1 shorter than first2: first1 exhausts while we are mid-first2-segment.
// Validates that seg2 walker composes the partial-segment first2 position on
// return so the top-level segmented_copy can flush the leftover first2 tail.
void test_merge_seg2_walker_first2_longer()
{
   int a[] = {3, 5};
   test_detail::seg_vector<int> sv2;
   int b1[] = {1, 2};
   int b2[] = {4, 6, 8};
   int b3[] = {10, 12};
   sv2.add_segment_range(b1, b1 + 2);
   sv2.add_segment_range(b2, b2 + 3);
   sv2.add_segment_range(b3, b3 + 2);

   boost::container::vector<int> out(9, 0);
   boost::container::vector<int>::iterator r = segmented_merge(
      a, a + 2, sv2.begin(), sv2.end(), out.begin());

   BOOST_TEST_EQ(static_cast<std::size_t>(r - out.begin()), 9u);
   int expected[] = {1, 2, 3, 4, 5, 6, 8, 10, 12};
   for(std::size_t i = 0; i < 9; ++i)
      BOOST_TEST_EQ(out[i], expected[i]);
}

// Stress: long flat first1 + multi-segmented first2 with non-uniform segments
// + segmented dst.  Each first2 segment gets fed flat into the bounded leaf,
// so the unrolled / dual-RA fast paths fire on every segment with size >= 4.
// Total elements 64 + 64 = 128, exercising the unrolled body multiple times.
void test_merge_seg2_walker_stress()
{
   const int N1 = 64;
   const int N2 = 64;
   boost::container::vector<int> v1;
   v1.reserve(N1);
   for(int i = 0; i < N1; ++i) v1.push_back(2 * i);              // 0, 2, 4, ...

   test_detail::seg_vector<int> sv2;
   int seg_sizes[] = {17, 9, 23, 4, 11};   // sums to 64
   int next = 1;
   for(std::size_t s = 0; s < sizeof(seg_sizes)/sizeof(seg_sizes[0]); ++s) {
      boost::container::vector<int> seg;
      seg.reserve(static_cast<std::size_t>(seg_sizes[s]));
      for(int j = 0; j < seg_sizes[s]; ++j) {
         seg.push_back(next);
         next += 2;
      }
      sv2.add_segment_range(seg.begin(), seg.end());
   }

   const int total = N1 + N2;
   test_detail::seg_vector<int> out;
   out.add_segment(11, 0);
   out.add_segment(50, 0);
   out.add_segment(40, 0);
   out.add_segment(total - 11 - 50 - 40, 0);

   typedef test_detail::seg_vector<int>::iterator iter_t;
   iter_t r = segmented_merge(
      v1.begin(), v1.end(), sv2.begin(), sv2.end(), out.begin());

   BOOST_TEST(r == out.end());
   iter_t it = out.begin();
   for(int i = 0; i < total; ++i, ++it)
      BOOST_TEST_EQ(*it, i);
}

// Asymmetric stress: src1 and src2 of very different sizes plus pathological
// data distribution where one source dominates a long contiguous run.
// Forces the count-based tail of the unrolled overload to handle the
// remaining > 4 elements once one side dropped below 4 in the main loop.
void test_merge_stress_asymmetric()
{
   boost::container::vector<int> v1, v2;
   for(int i = 0; i < 5; ++i) v1.push_back(1000 + i);            // [1000..1004]
   for(int i = 0; i < 50; ++i) v2.push_back(i);                  // [0..49]

   boost::container::vector<int> out(55, -1);
   boost::container::vector<int>::iterator r = segmented_merge(
      v1.begin(), v1.end(), v2.begin(), v2.end(), out.begin());

   BOOST_TEST_EQ(static_cast<std::size_t>(r - out.begin()), 55u);
   for(int i = 0; i < 50; ++i)
      BOOST_TEST_EQ(out[static_cast<std::size_t>(i)], i);
   for(int i = 0; i < 5; ++i)
      BOOST_TEST_EQ(out[static_cast<std::size_t>(50 + i)], 1000 + i);
}

int main()
{
   test_merge_segmented_inputs();
   test_merge_first_empty();
   test_merge_second_empty();
   test_merge_both_empty();
   test_merge_with_comp();
   test_merge_non_segmented();
   test_merge_duplicates();
   test_merge_sentinel_segmented();
   test_merge_sentinel_non_segmented();
   test_merge_seg2();
   test_merge_segmented_output();
   test_merge_segmented_inputs_and_output();
   test_merge_segmented_output_first_longer();
   test_merge_segmented_output_second_longer();
   test_merge_seg2_walker_flat_dst();
   test_merge_seg2_walker_seg_dst();
   test_merge_seg2_walker_first1_longer();
   test_merge_seg2_walker_first2_longer();
   test_merge_seg2_walker_stress();
   test_merge_stress_long_ranges();
   test_merge_stress_asymmetric();
   return boost::report_errors();
}

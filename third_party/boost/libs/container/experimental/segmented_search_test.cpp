//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_search.hpp>
#include <boost/core/lightweight_test.hpp>
#include "segmented_test_helper.hpp"
#include <boost/container/vector.hpp>

using namespace boost::container;

void test_search_found()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);

   int pattern[] = {3, 4, 5};
   test_detail::seg_vector<int>::iterator it =
      segmented_search(sv.begin(), sv.end(), pattern, pattern + 3);
   BOOST_TEST(it != sv.end());
   BOOST_TEST_EQ(*it, 3);
}

void test_search_not_found()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);

   int pattern[] = {3, 5};
   test_detail::seg_vector<int>::iterator it =
      segmented_search(sv.begin(), sv.end(), pattern, pattern + 2);
   BOOST_TEST(it == sv.end());
}

void test_search_empty_pattern()
{
   test_detail::seg_vector<int> sv;
   int a[] = {1, 2, 3};
   sv.add_segment_range(a, a + 3);

   int* empty = a;
   test_detail::seg_vector<int>::iterator it =
      segmented_search(sv.begin(), sv.end(), empty, empty);
   BOOST_TEST(it == sv.begin());
}

void test_search_at_end()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2};
   int a2[] = {3, 4};
   sv.add_segment_range(a1, a1 + 2);
   sv.add_segment_range(a2, a2 + 2);

   int pattern[] = {3, 4};
   test_detail::seg_vector<int>::iterator it =
      segmented_search(sv.begin(), sv.end(), pattern, pattern + 2);
   BOOST_TEST(it != sv.end());
   BOOST_TEST_EQ(*it, 3);
}

void test_search_non_segmented()
{
   int src[] = {1, 2, 3, 4, 5};
   boost::container::vector<int> v(src, src + 5);
   int pattern[] = {2, 3, 4};
   boost::container::vector<int>::iterator it = segmented_search(v.begin(), v.end(), pattern, pattern + 3);
   BOOST_TEST(it != v.end());
   BOOST_TEST_EQ(*it, 2);
}

void test_search_sentinel_segmented()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);

   int pattern[] = {3, 4, 5};
   test_detail::seg_vector<int>::iterator it =
      segmented_search(sv.begin(), test_detail::make_sentinel(sv.end()),
                       pattern, test_detail::make_sentinel(pattern + 3));
   BOOST_TEST(it != sv.end());
   BOOST_TEST_EQ(*it, 3);
}

void test_search_sentinel_non_segmented()
{
   int src[] = {1, 2, 3, 4, 5};
   boost::container::vector<int> v(src, src + 5);
   int pattern[] = {2, 3, 4};
   boost::container::vector<int>::iterator it =
      segmented_search(v.begin(), test_detail::make_sentinel(v.end()),
                       pattern, test_detail::make_sentinel(pattern + 3));
   BOOST_TEST(it != v.end());
   BOOST_TEST_EQ(*it, 2);
}

void test_search_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 2);
   sv2.add_flat_segment_range(a3, a3 + 4);

   int pattern[] = {4, 5, 6};
   test_detail::seg2_vector<int>::iterator it =
      segmented_search(sv2.begin(), sv2.end(), pattern, pattern + 3);
   BOOST_TEST(it != sv2.end());
   BOOST_TEST_EQ(*it, 4);
}

void test_search_every_position()
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

   iter_t expected = sv.begin();
   for(int i = 0; i < N; ++i, ++expected) {
      iter_t it = segmented_search(sv.begin(), sv.end(), vals + i, vals + i + 1);
      BOOST_TEST(it != sv.end());
      BOOST_TEST_EQ(*it, vals[i]);
      BOOST_TEST(it == expected);
   }

   int notfound = 999;
   BOOST_TEST(segmented_search(sv.begin(), sv.end(), &notfound, &notfound + 1) == sv.end());
}

void test_search_every_position_seg2()
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

   iter_t expected = sv2.begin();
   for(int i = 0; i < N; ++i, ++expected) {
      iter_t it = segmented_search(sv2.begin(), sv2.end(), vals + i, vals + i + 1);
      BOOST_TEST(it != sv2.end());
      BOOST_TEST_EQ(*it, vals[i]);
      BOOST_TEST(it == expected);
   }

   int notfound = 999;
   BOOST_TEST(segmented_search(sv2.begin(), sv2.end(), &notfound, &notfound + 1) == sv2.end());
}

//----------------------------------------------------------------------------
// Tests where the needle is itself a segmented iterator. These exercise the
// dual-segmentation path in segmented_search_bounded_mismatch.
//----------------------------------------------------------------------------

// Simple segmented needle inside a segmented haystack (single segment each).
void test_search_segmented_needle_simple()
{
   test_detail::seg_vector<int> hay;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6, 7};
   hay.add_segment_range(a1, a1 + 3);
   hay.add_segment_range(a2, a2 + 4);

   test_detail::seg_vector<int> ndl;
   int p[] = {5, 6};
   ndl.add_segment_range(p, p + 2);

   test_detail::seg_vector<int>::iterator it =
      segmented_search(hay.begin(), hay.end(), ndl.begin(), ndl.end());
   BOOST_TEST(it != hay.end());
   BOOST_TEST_EQ(*it, 5);
}

// Segmented needle whose contents straddle multiple needle segments.
void test_search_segmented_needle_multi_segment()
{
   test_detail::seg_vector<int> hay;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6};
   int a3[] = {7, 8, 9};
   hay.add_segment_range(a1, a1 + 3);
   hay.add_segment_range(a2, a2 + 3);
   hay.add_segment_range(a3, a3 + 3);

   test_detail::seg_vector<int> ndl;
   int p1[] = {3, 4};
   int p2[] = {5, 6, 7};
   ndl.add_segment_range(p1, p1 + 2);
   ndl.add_segment_range(p2, p2 + 3);

   test_detail::seg_vector<int>::iterator it =
      segmented_search(hay.begin(), hay.end(), ndl.begin(), ndl.end());
   BOOST_TEST(it != hay.end());
   BOOST_TEST_EQ(*it, 3);
}

// Match straddles both haystack and needle segment boundaries simultaneously.
void test_search_segmented_both_straddle()
{
   test_detail::seg_vector<int> hay;
   int a1[] = {10, 20};
   int a2[] = {30, 40};
   int a3[] = {50, 60};
   hay.add_segment_range(a1, a1 + 2);
   hay.add_segment_range(a2, a2 + 2);
   hay.add_segment_range(a3, a3 + 2);

   test_detail::seg_vector<int> ndl;
   int p1[] = {20, 30};
   int p2[] = {40, 50};
   ndl.add_segment_range(p1, p1 + 2);
   ndl.add_segment_range(p2, p2 + 2);

   test_detail::seg_vector<int>::iterator it =
      segmented_search(hay.begin(), hay.end(), ndl.begin(), ndl.end());
   BOOST_TEST(it != hay.end());
   BOOST_TEST_EQ(*it, 20);
}

void test_search_segmented_needle_not_found()
{
   test_detail::seg_vector<int> hay;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6};
   hay.add_segment_range(a1, a1 + 3);
   hay.add_segment_range(a2, a2 + 3);

   test_detail::seg_vector<int> ndl;
   int p1[] = {2, 3};
   int p2[] = {5};
   ndl.add_segment_range(p1, p1 + 2);
   ndl.add_segment_range(p2, p2 + 1);

   test_detail::seg_vector<int>::iterator it =
      segmented_search(hay.begin(), hay.end(), ndl.begin(), ndl.end());
   BOOST_TEST(it == hay.end());
}

// Empty segmented needle must return begin() per std::search contract.
void test_search_segmented_empty_needle()
{
   test_detail::seg_vector<int> hay;
   int a[] = {1, 2, 3};
   hay.add_segment_range(a, a + 3);

   test_detail::seg_vector<int> ndl;   // empty segmented range

   test_detail::seg_vector<int>::iterator it =
      segmented_search(hay.begin(), hay.end(), ndl.begin(), ndl.end());
   BOOST_TEST(it == hay.begin());
}

// Needle at the very end of the haystack, both segmented and straddling.
void test_search_segmented_needle_at_end()
{
   test_detail::seg_vector<int> hay;
   int a1[] = {1, 2};
   int a2[] = {3, 4};
   int a3[] = {5, 6};
   hay.add_segment_range(a1, a1 + 2);
   hay.add_segment_range(a2, a2 + 2);
   hay.add_segment_range(a3, a3 + 2);

   test_detail::seg_vector<int> ndl;
   int p1[] = {4};
   int p2[] = {5, 6};
   ndl.add_segment_range(p1, p1 + 1);
   ndl.add_segment_range(p2, p2 + 2);

   test_detail::seg_vector<int>::iterator it =
      segmented_search(hay.begin(), hay.end(), ndl.begin(), ndl.end());
   BOOST_TEST(it != hay.end());
   BOOST_TEST_EQ(*it, 4);
}

// Many false starts: haystack has repeated near-matches of the needle head.
// Exercises the find-then-verify outer loop across both sides' segments.
void test_search_segmented_false_starts()
{
   test_detail::seg_vector<int> hay;
   int a1[] = {1, 2, 1, 2};
   int a2[] = {1, 2, 1};
   int a3[] = {2, 3, 9};
   hay.add_segment_range(a1, a1 + 4);
   hay.add_segment_range(a2, a2 + 3);
   hay.add_segment_range(a3, a3 + 3);

   test_detail::seg_vector<int> ndl;
   int p1[] = {1, 2};
   int p2[] = {3};
   ndl.add_segment_range(p1, p1 + 2);
   ndl.add_segment_range(p2, p2 + 1);

   // The only match is the last "1,2" across a2/a3 boundary followed by '3'.
   test_detail::seg_vector<int>::iterator it =
      segmented_search(hay.begin(), hay.end(), ndl.begin(), ndl.end());
   BOOST_TEST(it != hay.end());
   BOOST_TEST_EQ(*it, 1);
   ++it; BOOST_TEST_EQ(*it, 2);
   ++it; BOOST_TEST_EQ(*it, 3);
}

// Two-level segmented haystack (seg2_vector) with a one-level segmented
// needle that straddles inner seg2 boundaries.
void test_search_seg2_haystack_segmented_needle()
{
   test_detail::seg2_vector<int> hay;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   hay.add_flat_segment_range(a1, a1 + 3);
   hay.add_flat_segment_range(a2, a2 + 2);
   hay.add_flat_segment_range(a3, a3 + 4);

   test_detail::seg_vector<int> ndl;
   int p1[] = {3, 4};
   int p2[] = {5, 6};
   ndl.add_segment_range(p1, p1 + 2);
   ndl.add_segment_range(p2, p2 + 2);

   test_detail::seg2_vector<int>::iterator it =
      segmented_search(hay.begin(), hay.end(), ndl.begin(), ndl.end());
   BOOST_TEST(it != hay.end());
   BOOST_TEST_EQ(*it, 3);
}

// Two-level segmented on both sides.
void test_search_seg2_haystack_seg2_needle()
{
   test_detail::seg2_vector<int> hay;
   int h1[] = {10, 20, 30};
   int h2[] = {40, 50};
   int h3[] = {60, 70, 80, 90};
   hay.add_flat_segment_range(h1, h1 + 3);
   hay.add_flat_segment_range(h2, h2 + 2);
   hay.add_flat_segment_range(h3, h3 + 4);

   test_detail::seg2_vector<int> ndl;
   int p1[] = {30, 40};
   int p2[] = {50};
   int p3[] = {60};
   ndl.add_flat_segment_range(p1, p1 + 2);
   ndl.add_flat_segment_range(p2, p2 + 1);
   ndl.add_flat_segment_range(p3, p3 + 1);

   test_detail::seg2_vector<int>::iterator it =
      segmented_search(hay.begin(), hay.end(), ndl.begin(), ndl.end());
   BOOST_TEST(it != hay.end());
   BOOST_TEST_EQ(*it, 30);
}

// Search every single-element segmented needle position against a segmented
// haystack, mirroring test_search_every_position but with a segmented needle.
void test_search_every_position_segmented_needle()
{
   test_detail::seg_vector<int> hay;
   int a1[] = {10, 20, 30};
   int a2[] = {40, 50};
   int a3[] = {60, 70, 80, 90};
   hay.add_segment_range(a1, a1 + 3);
   hay.add_segment_range(a2, a2 + 2);
   hay.add_segment_range(a3, a3 + 4);

   int vals[] = {10, 20, 30, 40, 50, 60, 70, 80, 90};
   const int N = 9;
   typedef test_detail::seg_vector<int>::iterator iter_t;

   iter_t expected = hay.begin();
   for(int i = 0; i < N; ++i, ++expected) {
      test_detail::seg_vector<int> ndl;
      ndl.add_segment_range(vals + i, vals + i + 1);
      iter_t it = segmented_search(hay.begin(), hay.end(), ndl.begin(), ndl.end());
      BOOST_TEST(it != hay.end());
      BOOST_TEST_EQ(*it, vals[i]);
      BOOST_TEST(it == expected);
   }

   test_detail::seg_vector<int> notfound;
   int nf = 999;
   notfound.add_segment_range(&nf, &nf + 1);
   BOOST_TEST(segmented_search(hay.begin(), hay.end(),
                               notfound.begin(), notfound.end()) == hay.end());
}

// Needle spans the *entire* haystack.
void test_search_segmented_needle_full_haystack()
{
   test_detail::seg_vector<int> hay;
   int h1[] = {1, 2, 3};
   int h2[] = {4, 5};
   hay.add_segment_range(h1, h1 + 3);
   hay.add_segment_range(h2, h2 + 2);

   test_detail::seg_vector<int> ndl;
   int p1[] = {1, 2};
   int p2[] = {3, 4, 5};
   ndl.add_segment_range(p1, p1 + 2);
   ndl.add_segment_range(p2, p2 + 3);

   test_detail::seg_vector<int>::iterator it =
      segmented_search(hay.begin(), hay.end(), ndl.begin(), ndl.end());
   BOOST_TEST(it == hay.begin());
}

// Needle longer than the haystack must not report a match.
void test_search_segmented_needle_longer_than_haystack()
{
   test_detail::seg_vector<int> hay;
   int h[] = {1, 2, 3};
   hay.add_segment_range(h, h + 3);

   test_detail::seg_vector<int> ndl;
   int p1[] = {1, 2};
   int p2[] = {3, 4};
   ndl.add_segment_range(p1, p1 + 2);
   ndl.add_segment_range(p2, p2 + 2);

   test_detail::seg_vector<int>::iterator it =
      segmented_search(hay.begin(), hay.end(), ndl.begin(), ndl.end());
   BOOST_TEST(it == hay.end());
}

// Non-segmented haystack with a segmented needle.
void test_search_flat_haystack_segmented_needle()
{
   int src[] = {1, 2, 3, 4, 5};
   boost::container::vector<int> hay(src, src + 5);

   test_detail::seg_vector<int> ndl;
   int p1[] = {2, 3};
   int p2[] = {4};
   ndl.add_segment_range(p1, p1 + 2);
   ndl.add_segment_range(p2, p2 + 1);

   boost::container::vector<int>::iterator it =
      segmented_search(hay.begin(), hay.end(), ndl.begin(), ndl.end());
   BOOST_TEST(it != hay.end());
   BOOST_TEST_EQ(*it, 2);
}

// Sentinel on both segmented ranges.
void test_search_segmented_both_sentinel()
{
   test_detail::seg_vector<int> hay;
   int h1[] = {1, 2, 3};
   int h2[] = {4, 5, 6};
   hay.add_segment_range(h1, h1 + 3);
   hay.add_segment_range(h2, h2 + 3);

   test_detail::seg_vector<int> ndl;
   int p1[] = {3, 4};
   int p2[] = {5};
   ndl.add_segment_range(p1, p1 + 2);
   ndl.add_segment_range(p2, p2 + 1);

   test_detail::seg_vector<int>::iterator it =
      segmented_search(hay.begin(), test_detail::make_sentinel(hay.end()),
                       ndl.begin(), test_detail::make_sentinel(ndl.end()));
   BOOST_TEST(it != hay.end());
   BOOST_TEST_EQ(*it, 3);
}

int main()
{
   test_search_found();
   test_search_not_found();
   test_search_empty_pattern();
   test_search_at_end();
   test_search_non_segmented();
   test_search_sentinel_segmented();
   test_search_sentinel_non_segmented();
   test_search_seg2();
   test_search_every_position();
   test_search_every_position_seg2();
   //Tests exercising the dual-segmentation path (segmented needle).
   test_search_segmented_needle_simple();
   test_search_segmented_needle_multi_segment();
   test_search_segmented_both_straddle();
   test_search_segmented_needle_not_found();
   test_search_segmented_empty_needle();
   test_search_segmented_needle_at_end();
   test_search_segmented_false_starts();
   test_search_seg2_haystack_segmented_needle();
   test_search_seg2_haystack_seg2_needle();
   test_search_every_position_segmented_needle();
   test_search_segmented_needle_full_haystack();
   test_search_segmented_needle_longer_than_haystack();
   test_search_flat_haystack_segmented_needle();
   test_search_segmented_both_sentinel();
   return boost::report_errors();
}

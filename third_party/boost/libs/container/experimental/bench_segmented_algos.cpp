//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

// Force aggressive function/loop alignment to eliminate instruction cache-line
// alignment noise from benchmark measurements. Without this, identical code can
// show up to 1.8x performance variation depending on where the linker happens
// to place each template instantiation relative to 64-byte cache-line boundaries.
// Requires GCC >= 9: GCC 8's #pragma GCC optimize applies optimization attributes
// to all functions in the TU, conflicting with __attribute__((always_inline)) on
// friend operators defined in headers (e.g. wrapped_iterator), causing
// -Werror=attributes errors.
#if defined(__GNUC__) && !defined(__clang__) && (__GNUC__ >= 9)
   #pragma GCC optimize("align-functions=64", "align-loops=32")
#elif defined(__clang__)
   // Clang has no file-wide pragma for alignment. Use command-line flags:
   //   -falign-functions=64 -falign-loops=32
#elif defined(_MSC_VER)
   // MSVC has no pragma or attribute for function/loop alignment control.
#endif

#include <algorithm>
#include <boost/container/vector.hpp>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstring>
#include <utility>
#include <typeinfo>

#include <boost/container/deque.hpp>
#include <boost/container/experimental/nest.hpp>

#include <boost/container/experimental/segmented_all_of.hpp>
#include <boost/container/experimental/segmented_any_of.hpp>
#include <boost/container/experimental/segmented_copy.hpp>
#include <boost/container/experimental/segmented_copy_if.hpp>
#include <boost/container/experimental/segmented_copy_n.hpp>
#include <boost/container/experimental/segmented_count.hpp>
#include <boost/container/experimental/segmented_count_if.hpp>
#include <boost/container/experimental/segmented_equal.hpp>
#include <boost/container/experimental/segmented_fill.hpp>
#include <boost/container/experimental/segmented_fill_n.hpp>
#include <boost/container/experimental/segmented_find.hpp>
#include <boost/container/experimental/segmented_find_if.hpp>
#include <boost/container/experimental/segmented_find_if_not.hpp>
#include <boost/container/experimental/segmented_find_last.hpp>
#include <boost/container/experimental/segmented_find_last_if.hpp>
#include <boost/container/experimental/segmented_find_last_if_not.hpp>
#include <boost/container/experimental/segmented_for_each.hpp>
#include <boost/container/experimental/segmented_generate.hpp>
#include <boost/container/experimental/segmented_generate_n.hpp>
#include <boost/container/experimental/segmented_is_partitioned.hpp>
#include <boost/container/experimental/segmented_is_sorted.hpp>
#include <boost/container/experimental/segmented_merge.hpp>
#include <boost/container/experimental/segmented_mismatch.hpp>
#include <boost/container/experimental/segmented_none_of.hpp>
#include <boost/container/experimental/segmented_is_sorted_until.hpp>
#include <boost/container/experimental/segmented_partition.hpp>
#include <boost/container/experimental/segmented_partition_copy.hpp>
#include <boost/container/experimental/segmented_partition_point.hpp>
#include <boost/container/experimental/segmented_remove.hpp>
#include <boost/container/experimental/segmented_remove_if.hpp>
#include <boost/container/experimental/segmented_remove_copy.hpp>
#include <boost/container/experimental/segmented_remove_copy_if.hpp>
#include <boost/container/experimental/segmented_replace.hpp>
#include <boost/container/experimental/segmented_replace_if.hpp>
#include <boost/container/experimental/segmented_reverse.hpp>
#include <boost/container/experimental/segmented_reverse_copy.hpp>
#include <boost/container/experimental/segmented_search.hpp>
#include <boost/container/experimental/segmented_search_n.hpp>
#include <boost/container/experimental/segmented_set_difference.hpp>
#include <boost/container/experimental/segmented_set_intersection.hpp>
#include <boost/container/experimental/segmented_set_symmetric_difference.hpp>
#include <boost/container/experimental/segmented_set_union.hpp>
#include <boost/container/experimental/segmented_stable_partition.hpp>
#include <boost/container/experimental/segmented_swap_ranges.hpp>
#include <boost/container/experimental/segmented_transform.hpp>
#include <boost/container/experimental/wrapped_iterator.hpp>
#include "../bench/bench_utils.hpp"

#define BOOST_CONTAINER_BENCH_SEGMENTED_GROUP 10
#define BOOST_CONTAINER_BENCH_SEGMENTED_BIDIR_ENABLED 0

namespace bc = boost::container;

//////////////////////////////////////////////////////////////////////////////
// Value types
//////////////////////////////////////////////////////////////////////////////

class MyInt
{
   int int_;

   public:
   inline explicit MyInt(int i = 0)
      : int_(i)
   {}

   inline MyInt(const MyInt &other)
      :  int_(other.int_)
   {}

   inline MyInt & operator=(const MyInt &other)
   {
      int_ = other.int_;
      return *this;
   }

   inline ~MyInt()
   {
      int_ = 0;
   }

   inline int int_value() const { return int_; }

   friend inline bool operator==(const MyInt& a, const MyInt& b) { return a.int_ == b.int_; }
   friend inline bool operator!=(const MyInt& a, const MyInt& b) { return a.int_ != b.int_; }
   friend inline bool operator<(const MyInt& a, const MyInt& b) { return a.int_ < b.int_; }
   friend inline bool operator>(const MyInt& a, const MyInt& b) { return a.int_ > b.int_; }
};

class MyFatInt
{
   int int0_;
   int int1_;
   int int2_;
   int int3_;
   int int4_;
   int int5_;
   int int6_;
   int int7_;

   public:
   inline explicit MyFatInt(int i = 0)
      : int0_(i++)
      , int1_(i++)
      , int2_(i++)
      , int3_(i++)
      , int4_(i++)
      , int5_(i++)
      , int6_(i++)
      , int7_(i++)
   {}

   inline MyFatInt(const MyFatInt &other)
      : int0_(other.int0_)
      , int1_(other.int1_)
      , int2_(other.int2_)
      , int3_(other.int3_)
      , int4_(other.int4_)
      , int5_(other.int5_)
      , int6_(other.int6_)
      , int7_(other.int7_)
   {}

   inline MyFatInt & operator=(const MyFatInt &other)
   {
      int0_ = other.int0_;
      int1_ = other.int1_;
      int2_ = other.int2_;
      int3_ = other.int3_;
      int4_ = other.int4_;
      int5_ = other.int5_;
      int6_ = other.int6_;
      int7_ = other.int7_;
      return *this;
   }

   inline ~MyFatInt()
   {
      int0_ = 0;
      int1_ = 0;
      int2_ = 0;
      int3_ = 0;
      int4_ = 0;
      int5_ = 0;
      int6_ = 0;
      int7_ = 0;
   }

   inline int int_value() const { return int0_; }

   friend inline bool operator==(const MyFatInt& a, const MyFatInt& b) { return a.int0_ == b.int0_; }
   friend inline bool operator!=(const MyFatInt& a, const MyFatInt& b) { return a.int0_ != b.int0_; }
   friend inline bool operator<(const MyFatInt& a, const MyFatInt& b) { return a.int0_ < b.int0_; }
   friend inline bool operator>(const MyFatInt& a, const MyFatInt& b) { return a.int0_ > b.int0_; }
   friend inline bool operator<=(const MyFatInt& a, const MyFatInt& b) { return a.int0_ <= b.int0_; }
   friend inline bool operator>=(const MyFatInt& a, const MyFatInt& b) { return a.int0_ >= b.int0_; }
};

inline int int_value(int x) { return x; }
inline int int_value(const MyInt& x) { return x.int_value(); }
inline int int_value(const MyFatInt& x) { return x.int_value(); }

//////////////////////////////////////////////////////////////////////////////
// Functors
//////////////////////////////////////////////////////////////////////////////

template<class T>
struct add_one
{
   T operator()(const T& x) const { return T(int_value(x) + 1); }
};

template<class T>
struct is_odd
{
   bool operator()(const T& x) const { return (int_value(x) & 1) != 0; }
};

template<class T>
struct summer
{
   int sum;
   summer() : sum(0) {}
   void operator()(const T& x) { sum = static_cast<int>((static_cast<unsigned>(sum) + static_cast<unsigned>(int_value(x)))); }
};

template<class T>
struct is_negative
{
   bool operator()(const T& x) const { return int_value(x) < 0; }
};

template<class T>
struct is_zero_or_positive
{
   bool operator()(const T& x) const { return int_value(x) >= 0; }
};


template<class T>
struct counter
{
   int n;
   counter() : n(0) {}
   T operator()() { return T(n++); }
};

template<class T>
class equal_to_ref
{
   typedef T value_type;
   const value_type &t_;

   public:
   BOOST_CONTAINER_FORCEINLINE explicit equal_to_ref(const value_type &t)
      :  t_(t)
   {}

   template <class U>
   BOOST_CONTAINER_FORCEINLINE bool operator()(const U &t)const
   {
      return t_ == t;
   }
};

template<class T>
class less_than_ref
{
   typedef T value_type;
   const value_type& t_;
public:
   BOOST_CONTAINER_FORCEINLINE explicit less_than_ref(const value_type& t)
      : t_(t)
   {
   }
   template <class U>
   BOOST_CONTAINER_FORCEINLINE bool operator()(const U& t)const
   {
      return t < t_;
   }
};

template<class T>
class less_and_greater_ref
{
   typedef T value_type;
   const value_type& l_;
   const value_type& g_;
public:
   BOOST_CONTAINER_FORCEINLINE explicit less_and_greater_ref(const value_type& l, const value_type& g)
      : l_(l), g_(g)
   {
   }
   template <class U>
   BOOST_CONTAINER_FORCEINLINE bool operator()(const U& t)const
   {
      return t < l_ || t > g_ ;
   }
};

template<class T>
class unequal_to_ref
{
   typedef T value_type;
   const value_type &t_;

   public:
   BOOST_CONTAINER_FORCEINLINE explicit unequal_to_ref(const value_type &t)
      :  t_(t)
   {}

   template <class U>
   BOOST_CONTAINER_FORCEINLINE bool operator()(const U &t)const
   {
      return t_ != t;
   }
};

//////////////////////////////////////////////////////////////////////////////
// C++03 fallbacks for C++11-only <algorithm> functions
//////////////////////////////////////////////////////////////////////////////

namespace bench_detail {

#if BOOST_CXX_VERSION < 201103L

template<class InIt, class Pred>
bool all_of(InIt first, InIt last, Pred pred)
{
   for (; first != last; ++first)
      if (!pred(*first)) return false;
   return true;
}

template<class InIt, class Pred>
bool any_of(InIt first, InIt last, Pred pred)
{
   for (; first != last; ++first)
      if (pred(*first)) return true;
   return false;
}

template<class InIt, class Pred>
bool none_of(InIt first, InIt last, Pred pred)
{
   for (; first != last; ++first)
      if (pred(*first)) return false;
   return true;
}

template <class InpIter, class Sent, class Pred>
inline InpIter find_if_not(InpIter first, Sent last, Pred pred)
{
   return std::find_if(first, last, boost::container::not_pred<Pred>(pred));
}

template<class InIt, class OutIt, class Pred>
OutIt copy_if(InIt first, InIt last, OutIt d_first, Pred pred)
{
   for (; first != last; ++first)
      if (pred(*first))
         *d_first++ = *first;
   return d_first;
}

template<class InIt, class Size, class OutIt>
OutIt copy_n(InIt first, Size count, OutIt result)
{
   for (Size i = 0; i < count; ++i, ++first, ++result)
      *result = *first;
   return result;
}

template<class FwdIt>
bool is_sorted(FwdIt first, FwdIt last)
{
   if (first != last) {
      FwdIt next = first;
      for (; ++next != last; first = next)
         if (*next < *first)
            return false;
   }
   return true;
}

template<class FwdIt>
FwdIt is_sorted_until(FwdIt first, FwdIt last)
{
   if (first != last) {
      FwdIt next = first;
      for (; ++next != last; first = next)
         if (*next < *first)
            return next;
   }
   return last;
}

template<class InIt, class Pred>
bool is_partitioned(InIt first, InIt last, Pred pred)
{
   for (; first != last; ++first)
      if (!pred(*first)) break;
   for (; first != last; ++first)
      if (pred(*first)) return false;
   return true;
}

template<class InIt, class OutIt1, class OutIt2, class Pred>
std::pair<OutIt1, OutIt2>
partition_copy(InIt first, InIt last,
               OutIt1 out_true, OutIt2 out_false, Pred pred)
{
   for (; first != last; ++first) {
      if (pred(*first))
         *out_true++ = *first;
      else
         *out_false++ = *first;
   }
   return std::pair<OutIt1, OutIt2>(out_true, out_false);
}

template<class FwdIt, class Pred>
FwdIt partition_point(FwdIt first, FwdIt last, Pred pred)
{
   for (; first != last; ++first)
      if (!pred(*first)) return first;
   return last;
}

#else

   using std::all_of;
   using std::any_of;
   using std::none_of;
   using std::find_if_not;
   using std::copy_if;
   using std::copy_n;
   using std::is_sorted;
   using std::is_sorted_until;
   using std::is_partitioned;
   using std::partition_copy;
   using std::partition_point;

#endif

#if BOOST_CXX_VERSION < 201402L

// Portable two-range std::mismatch (std::mismatch with 4 args is C++14).
template<class InIt1, class InIt2>
std::pair<InIt1, InIt2> mismatch(InIt1 first1, InIt1 last1, InIt2 first2, InIt2 last2)
{
   while (first1 != last1 && first2 != last2 && *first1 == *first2) {
      ++first1; ++first2;
   }
   return std::pair<InIt1, InIt2>(first1, first2);
}

#else

using std::mismatch;

#endif

template<class BidirIt, class T>
BidirIt find_last_dispatch(BidirIt first, BidirIt last, const T& val, std::bidirectional_iterator_tag)
{
   BidirIt it = last;
   while (it != first) {
      --it;
      if (*it == val) return it;
   }
   return last;
}

template<class FwdIt, class T>
FwdIt find_last_dispatch(FwdIt first, FwdIt last, const T& val, std::forward_iterator_tag)
{
   FwdIt result = last;
   for (; first != last; ++first)
      if (*first == val) result = first;
   return result;
}

template<class It, class T>
BOOST_CONTAINER_FORCEINLINE It find_last(It first, It last, const T& val)
{
   typedef typename std::iterator_traits<It>::iterator_category cat;
   return find_last_dispatch(first, last, val, cat());
}

template<class BidirIt, class Pred>
BidirIt find_last_if_dispatch(BidirIt first, BidirIt last, Pred pred, std::bidirectional_iterator_tag)
{
   BidirIt it = last;
   while (it != first) {
      --it;
      if (pred(*it)) return it;
   }
   return last;
}

template<class FwdIt, class Pred>
FwdIt find_last_if_dispatch(FwdIt first, FwdIt last, Pred pred, std::forward_iterator_tag)
{
   FwdIt result = last;
   for (; first != last; ++first)
      if (pred(*first)) result = first;
   return result;
}

template<class It, class Pred>
BOOST_CONTAINER_FORCEINLINE It find_last_if(It first, It last, Pred pred)
{
   typedef typename std::iterator_traits<It>::iterator_category cat;
   return find_last_if_dispatch(first, last, pred, cat());
}

template<class BidirIt, class Pred>
BidirIt find_last_if_not_dispatch(BidirIt first, BidirIt last, Pred pred, std::bidirectional_iterator_tag)
{
   BidirIt it = last;
   while (it != first) {
      --it;
      if (!pred(*it)) return it;
   }
   return last;
}

template<class FwdIt, class Pred>
FwdIt find_last_if_not_dispatch(FwdIt first, FwdIt last, Pred pred, std::forward_iterator_tag)
{
   FwdIt result = last;
   for (; first != last; ++first)
      if (!pred(*first)) result = first;
   return result;
}

template<class It, class Pred>
BOOST_CONTAINER_FORCEINLINE It find_last_if_not(It first, It last, Pred pred)
{
   typedef typename std::iterator_traits<It>::iterator_category cat;
   return find_last_if_not_dispatch(first, last, pred, cat());
}

//Not benchmarked:
//inplace_merge


//not implemented (c++03)
//find_end
//find_first_of
//adjacent_find
//copy_backward
//move
//move_backward
//transform
//replace_copy
//replace_copy_if
//unique
//unique_copy
//rotate
//rotate_copy
//max_element
//min_element
//minmax_element
//nth_element
//includes
//set_union
//lexicographical_compare
//   -- sorting --
//sort (random-access non-implementable?)
//stable_sort (random-access non-implementable?)
//partial_sort (random-access non-implementable?)
//partial_sort_copy(random-access non-implementable?)
//   -- binary search --
//lower_bound (binary/random-access non-implementable?)
//upper_bound (binary/random-access non-implementable?)
//equal_range (binary/random-access non-implementable?)
//binary_search (binary/random-access non-implementable?)
//   -- heap --
//push_heap (binary/random-access non-implementable?)
//pop_heap (binary/random-access non-implementable?)
//make_heap (binary/random-access non-implementable?)
//sort_heap (binary/random-access non-implementable?)
//   -- permutation --
//next_permutation
//prev_permutation
//is_permutation
//   -- numeric --
//iota
//accumulate
//inner_product
//adjacent_difference
//partial_sum

//not implemented (c++11):
//move
//move_backward
//shuffle (random-access non-implementable?)
//is_heap (random-access non-implementable?)
//is_heap_until (random-access non-implementable?)

//not implemented (c++14)
//equal with two full ranges

//not implemented (c++17)
//for_each_n
//random_shuffle (random-access non-implementable?)
//sample (random-access non-implementable?)
//reduce
//exclusive_scan
//inclusive_scan
//transform_reduce
//transform_exclusive_scan
//transform_inclusive_scan





//not implemented (c++20)
//shift_left
//shift_right
//lexicographical_compare_three_way

//range-based?
//contains
//contains_subrange
//starts_with
//ends_with
//fold_left
//fold_left_first
//fold_right
//fold_right_last
//fold_left_with_iter
//fold_left_first_with_iter
//generate_random


} // namespace bench_detail

//////////////////////////////////////////////////////////////////////////////
// Fill helpers
//////////////////////////////////////////////////////////////////////////////

template<class C>
void fill_test_data(C& c, std::size_t n)
{
   typedef typename C::value_type VT;
   for (std::size_t i = 0; i < n; ++i)
      c.push_back(VT(static_cast<int>(i)));
}

template<class T, class A, class O>
void fill_test_data(bc::nest<T,A,O>& c, std::size_t n)
{
   for (std::size_t i = 0; i < n; ++i)
      c.insert(T(static_cast<int>(i)));
}

//////////////////////////////////////////////////////////////////////////////
// Benchmark helpers
//////////////////////////////////////////////////////////////////////////////


inline double calc_ns_per_elem(boost::move_detail::nanosecond_type ns,
                               std::size_t iters, std::size_t elems)
{
   return double(ns) / double(iters * elems);
}

inline void print_subheader()
{
   std::cout << std::left  << std::setw(24) << "< algo >"
             << std::right << std::setw(16) << "< nsg/seg >"
             << std::right << std::setw(16) << "< std/seg >"
             << std::right << std::setw(16) << "< std/nsg >"
             << '\n';
}

struct geomean_accumulator
{
   double std_over_seg_log_sum, std_over_nsg_log_sum, ns_over_std_over_seg_log_sum;
   int    std_over_seg_count, std_over_nsg_count, nseg_over_seg_count;

   void reset()
   {
      std_over_seg_log_sum = std_over_nsg_log_sum = ns_over_std_over_seg_log_sum = 0.0;
      std_over_seg_count = std_over_nsg_count = nseg_over_seg_count = 0;
   }

   void add_std_over_seg(double r)
   {
      if(r > 0.0) {
         std_over_seg_log_sum += std::log(r);
         ++std_over_seg_count;
      }
   }

   void add_std_over_nsg(double r)
   {
      if(r > 0.0) {
         std_over_nsg_log_sum += std::log(r);
         ++std_over_nsg_count;
      }
   }

   void add_nsg_over_seg(double r)
   {
      if(r > 0.0) {
         ns_over_std_over_seg_log_sum += std::log(r);
         ++nseg_over_seg_count;
      }
   }

   double std_over_seg_result() const
   {
      return std_over_seg_count > 0 ? std::exp(std_over_seg_log_sum / std_over_seg_count) : 0.0;
   }

   double std_over_nsg_result() const
   {
      return std_over_nsg_count > 0 ? std::exp(std_over_nsg_log_sum / std_over_nsg_count) : 0.0;
   }

   double nsg_over_seg_result() const
   {
      return nseg_over_seg_count > 0 ? std::exp(ns_over_std_over_seg_log_sum / nseg_over_seg_count) : 0.0;
   }

} g_geomean = { 0.0, 0.0, 0.0, 0, 0, 0 };

inline void print_ratio(const char* algo, const char*,
                        double std_ns, double seg_ns, double nsg_ns)
{
   double nsg_seg_ratio   = (seg_ns > 0.0) ? nsg_ns / seg_ns : 0.0;
   double std_seg_ratio   = (seg_ns > 0.0) ? std_ns / seg_ns : 0.0;
   double std_nsg_ratio   = (std_ns > 0.0) ? std_ns / nsg_ns : 0.0;
   g_geomean.add_std_over_seg(std_seg_ratio);
   g_geomean.add_std_over_nsg(std_nsg_ratio);
   g_geomean.add_nsg_over_seg(nsg_seg_ratio);
   std::cout << std::left  << std::setw(24) << algo
             << std::right << std::setw(16) << std::fixed << std::setprecision(2) << nsg_seg_ratio
             << std::right << std::setw(16) << std::fixed << std::setprecision(2) << std_seg_ratio
             << std::right << std::setw(16) << std::fixed << std::setprecision(2) << std_nsg_ratio
             << '\n';
}

//////////////////////////////////////////////////////////////////////////////
// Measurement infrastructure
//////////////////////////////////////////////////////////////////////////////

template <class F, class ResetF>
inline boost::move_detail::nanosecond_type measure_batch(std::size_t iters, F f, ResetF reset_f)
{
   cpu_timer t;
   std::size_t n_ = (iters + 7) / 8;
   t.resume();
   switch (iters % 8) {
   case 0: do {
      t.resume();
      f(); BOOST_FALLTHROUGH;
   case 7: f(); BOOST_FALLTHROUGH;
   case 6: f(); BOOST_FALLTHROUGH;
   case 5: f(); BOOST_FALLTHROUGH;
   case 4: f(); BOOST_FALLTHROUGH;
   case 3: f(); BOOST_FALLTHROUGH;
   case 2: f(); BOOST_FALLTHROUGH;
   case 1: f();
      t.stop();
      if (BOOST_UNLIKELY(--n_ == 0)) break;
      reset_f();
   } while (true);
   }
   return t.elapsed();
}

struct noop_reset {
   BOOST_CONTAINER_FORCEINLINE void operator()() {}
};

template <class F1, class R1, class F2, class R2, class F3, class R3>
inline void compare_batch(std::size_t iters, std::size_t nelems,
                          F1 std_op, R1 std_reset, F2 seg_op, R2 seg_reset,
                          F3 nsg_op, R3 nsg_reset,
                          const char* label, const char* cname)
{
   typedef boost::move_detail::nanosecond_type ns_type;
   ns_type t1 = measure_batch(iters, std_op, std_reset);
   ns_type t2 = measure_batch(iters, seg_op, seg_reset);
   ns_type t3 = measure_batch(iters, nsg_op, nsg_reset);
   print_ratio(label, cname, calc_ns_per_elem(t1, iters, nelems),
               calc_ns_per_elem(t2, iters, nelems), calc_ns_per_elem(t3, iters, nelems));
}

template <class F1, class F2, class F3>
inline void compare_batch(std::size_t iters, std::size_t nelems,
                          F1 std_op, F2 seg_op, F3 nsg_op,
                          const char* label, const char* cname)
{
   compare_batch(iters, nelems, std_op, noop_reset(), seg_op, noop_reset(),
                 nsg_op, noop_reset(), label, cname);
}

//////////////////////////////////////////////////////////////////////////////
// Benchmark operation functors
//////////////////////////////////////////////////////////////////////////////

namespace bench_ops {

template<bool Wrap> struct iter_w {
   template<class It> static BOOST_CONTAINER_FORCEINLINE
   It wrap(It i) { return i; }
};
template<> struct iter_w<true> {
   template<class It> static BOOST_CONTAINER_FORCEINLINE
   bc::wrapped_iterator<It> wrap(It i) { return bc::wrapped_iterator<It>(i); }
};

template<bool Wrap, class It> struct iter_wt          { typedef It type; };
template<class It>            struct iter_wt<true, It> { typedef bc::wrapped_iterator<It> type; };

template<class C>
struct batch_state {
   C c1, c2, c3, c4, c5, c6, c7, c8;
   C* cs[8];
   int idx;
   const C &orig;
   batch_state(const C &c_)
      : c1(c_), c2(c_), c3(c_), c4(c_), c5(c_), c6(c_), c7(c_), c8(c_), idx(0), orig(c_)
   { cs[0]=&c1; cs[1]=&c2; cs[2]=&c3; cs[3]=&c4; cs[4]=&c5; cs[5]=&c6; cs[6]=&c7; cs[7]=&c8; }
};

template<class C>
struct batch_reset {
   batch_state<C> &bs;
   batch_reset(batch_state<C> &b) : bs(b) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { bs.idx = 0; bs.c1 = bs.c2 = bs.c3 = bs.c4 = bs.c5 = bs.c6 = bs.c7 = bs.c8 = bs.orig; }
};

// --- all_of ---
template<class C, class Pred>
struct std_all_of {
   const C &c; Pred pred; int &result;
   std_all_of(const C &c_, Pred p_, int &r_) : c(c_), pred(p_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); result = bench_detail::all_of(c.begin(), c.end(), pred) ? 1 : 0; escape(&result); }
};
template<class C, class Pred, bool Wrap = false>
struct seg_all_of {
   const C &c; Pred pred; int &result;
   seg_all_of(const C &c_, Pred p_, int &r_) : c(c_), pred(p_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); result = bc::segmented_all_of(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), pred) ? 1 : 0; escape(&result); }
};

// --- any_of ---
template<class C, class Pred>
struct std_any_of {
   const C &c; Pred pred; int &result;
   std_any_of(const C &c_, Pred p_, int &r_) : c(c_), pred(p_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); result = bench_detail::any_of(c.begin(), c.end(), pred) ? 1 : 0; escape(&result); }
};
template<class C, class Pred, bool Wrap = false>
struct seg_any_of {
   const C &c; Pred pred; int &result;
   seg_any_of(const C &c_, Pred p_, int &r_) : c(c_), pred(p_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); result = bc::segmented_any_of(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), pred) ? 1 : 0; escape(&result); }
};

// --- none_of ---
template<class C, class Pred>
struct std_none_of {
   const C &c; Pred pred; int &result;
   std_none_of(const C &c_, Pred p_, int &r_) : c(c_), pred(p_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); result = bench_detail::none_of(c.begin(), c.end(), pred) ? 1 : 0; escape(&result); }
};
template<class C, class Pred, bool Wrap = false>
struct seg_none_of {
   const C &c; Pred pred; int &result;
   seg_none_of(const C &c_, Pred p_, int &r_) : c(c_), pred(p_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); result = bc::segmented_none_of(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), pred) ? 1 : 0; escape(&result); }
};

// --- for_each ---
template<class C>
struct std_for_each {
   typedef typename C::value_type VT;
   const C &c; int &result;
   std_for_each(const C &c_, int &r_) : c(c_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { summer<VT> s; clobber(); s = std::for_each(c.begin(), c.end(), s); result = s.sum; escape(&result); }
};
template<class C, bool Wrap = false>
struct seg_for_each {
   typedef typename C::value_type VT;
   const C &c; int &result;
   seg_for_each(const C &c_, int &r_) : c(c_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { summer<VT> s; clobber(); s = bc::segmented_for_each(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), s); result = s.sum; escape(&result); }
};

// --- copy ---
template<class C, class OutT>
struct std_copy {
   const C &c; OutT &out;
   std_copy(const C &c_, OutT &o_) : c(c_), out(o_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); std::copy(c.begin(), c.end(), out.begin()); escape(&*out.begin()); }
};
template<class C, class OutT, bool Wrap = false>
struct seg_copy {
   const C &c; OutT &out;
   seg_copy(const C &c_, OutT &o_) : c(c_), out(o_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); bc::segmented_copy(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), iter_w<Wrap>::wrap(out.begin())); escape(&*out.begin()); }
};

// --- copy_if ---
template<class C, class OutT, class Pred>
struct std_copy_if {
   const C &c; OutT &out; Pred pred;
   std_copy_if(const C &c_, OutT &o_, Pred p_) : c(c_), out(o_), pred(p_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); bench_detail::copy_if(c.begin(), c.end(), out.begin(), pred); escape(&*out.begin()); }
};
template<class C, class OutT, class Pred, bool Wrap = false>
struct seg_copy_if {
   const C &c; OutT &out; Pred pred;
   seg_copy_if(const C &c_, OutT &o_, Pred p_) : c(c_), out(o_), pred(p_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); bc::segmented_copy_if(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), iter_w<Wrap>::wrap(out.begin()), pred); escape(&*out.begin()); }
};

// --- fill ---
template<class C>
struct std_fill {
   C &c2; const typename C::value_type &val;
   std_fill(C &c_, const typename C::value_type &v_) : c2(c_), val(v_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); std::fill(c2.begin(), c2.end(), val); escape(&c2); }
};
template<class C, bool Wrap = false>
struct seg_fill {
   C &c2; const typename C::value_type &val;
   seg_fill(C &c_, const typename C::value_type &v_) : c2(c_), val(v_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); bc::segmented_fill(iter_w<Wrap>::wrap(c2.begin()), iter_w<Wrap>::wrap(c2.end()), val); escape(&c2); }
};

// --- count ---
template<class C>
struct std_count {
   const C &c; const typename C::value_type &val; int &result;
   std_count(const C &c_, const typename C::value_type &v_, int &r_) : c(c_), val(v_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); result = static_cast<int>(std::count(c.begin(), c.end(), val)); escape(&result); }
};
template<class C, bool Wrap = false>
struct seg_count {
   const C &c; const typename C::value_type &val; int &result;
   seg_count(const C &c_, const typename C::value_type &v_, int &r_) : c(c_), val(v_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); result = static_cast<int>(bc::segmented_count(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), val)); escape(&result); }
};

// --- count_if ---
template<class C, class Pred>
struct std_count_if {
   const C &c; Pred pred; int &result;
   std_count_if(const C &c_, Pred p_, int &r_) : c(c_), pred(p_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); result = static_cast<int>(std::count_if(c.begin(), c.end(), pred)); escape(&result); }
};
template<class C, class Pred, bool Wrap = false>
struct seg_count_if {
   const C &c; Pred pred; int &result;
   seg_count_if(const C &c_, Pred p_, int &r_) : c(c_), pred(p_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); result = static_cast<int>(bc::segmented_count_if(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), pred)); escape(&result); }
};

// --- find ---
template<class C>
struct std_find {
   typedef typename C::const_iterator cit_t;
   const C &c; const typename C::value_type &val; int &result;
   std_find(const C &c_, const typename C::value_type &v_, int &r_) : c(c_), val(v_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); cit_t it = std::find(c.begin(), c.end(), val); result = (it == c.end()) ? 0 : 1; escape(&result); }
};
template<class C, bool Wrap = false>
struct seg_find {
   typedef typename iter_wt<Wrap, typename C::const_iterator>::type cit_t;
   const C &c; const typename C::value_type &val; int &result;
   seg_find(const C &c_, const typename C::value_type &v_, int &r_) : c(c_), val(v_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); cit_t it = bc::segmented_find(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), val); result = (it == iter_w<Wrap>::wrap(c.end())) ? 0 : 1; escape(&result); }
};

// --- find_if ---
template<class C, class Pred>
struct std_find_if {
   typedef typename C::const_iterator cit_t;
   const C &c; Pred pred; int &result;
   std_find_if(const C &c_, Pred p_, int &r_) : c(c_), pred(p_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); cit_t it = std::find_if(c.begin(), c.end(), pred); result = (it != c.end()) ? int_value(*it) : -1; escape(&result); }
};
template<class C, class Pred, bool Wrap = false>
struct seg_find_if {
   typedef typename iter_wt<Wrap, typename C::const_iterator>::type cit_t;
   const C &c; Pred pred; int &result;
   seg_find_if(const C &c_, Pred p_, int &r_) : c(c_), pred(p_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); cit_t it = bc::segmented_find_if(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), pred); result = (it != iter_w<Wrap>::wrap(c.end())) ? int_value(*it) : -1; escape(&result); }
};

// --- find_if_not ---
template<class C, class Pred>
struct std_find_if_not {
   typedef typename C::const_iterator cit_t;
   const C &c; Pred pred; int &result;
   std_find_if_not(const C &c_, Pred p_, int &r_) : c(c_), pred(p_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); cit_t it = bench_detail::find_if_not(c.begin(), c.end(), pred); result = (it != c.end()) ? int_value(*it) : -1; escape(&result); }
};
template<class C, class Pred, bool Wrap = false>
struct seg_find_if_not {
   typedef typename iter_wt<Wrap, typename C::const_iterator>::type cit_t;
   const C &c; Pred pred; int &result;
   seg_find_if_not(const C &c_, Pred p_, int &r_) : c(c_), pred(p_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); cit_t it = bc::segmented_find_if_not(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), pred); result = (it != iter_w<Wrap>::wrap(c.end())) ? int_value(*it) : -1; escape(&result); }
};

// --- find_last ---
template<class C>
struct std_find_last {
   typedef typename C::const_iterator cit_t;
   const C &c; const typename C::value_type &val; int &result;
   std_find_last(const C &c_, const typename C::value_type &v_, int &r_) : c(c_), val(v_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); cit_t it = bench_detail::find_last(c.begin(), c.end(), val); result = (it == c.end()) ? 0 : 1; escape(&result); }
};
template<class C, bool Wrap = false>
struct seg_find_last {
   typedef typename iter_wt<Wrap, typename C::const_iterator>::type cit_t;
   const C &c; const typename C::value_type &val; int &result;
   seg_find_last(const C &c_, const typename C::value_type &v_, int &r_) : c(c_), val(v_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); cit_t it = bc::segmented_find_last(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), val); result = (it == iter_w<Wrap>::wrap(c.end())) ? 0 : 1; escape(&result); }
};

// --- find_last_if ---
template<class C, class Pred>
struct std_find_last_if {
   typedef typename C::const_iterator cit_t;
   const C &c; Pred pred; int &result;
   std_find_last_if(const C &c_, Pred p_, int &r_) : c(c_), pred(p_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); cit_t it = bench_detail::find_last_if(c.begin(), c.end(), pred); result = (it != c.end()) ? int_value(*it) : -1; escape(&result); }
};
template<class C, class Pred, bool Wrap = false>
struct seg_find_last_if {
   typedef typename iter_wt<Wrap, typename C::const_iterator>::type cit_t;
   const C &c; Pred pred; int &result;
   seg_find_last_if(const C &c_, Pred p_, int &r_) : c(c_), pred(p_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); cit_t it = bc::segmented_find_last_if(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), pred); result = (it != iter_w<Wrap>::wrap(c.end())) ? int_value(*it) : -1; escape(&result); }
};

// --- find_last_if_not ---
template<class C, class Pred>
struct std_find_last_if_not {
   typedef typename C::const_iterator cit_t;
   const C &c; Pred pred; int &result;
   std_find_last_if_not(const C &c_, Pred p_, int &r_) : c(c_), pred(p_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); cit_t it = bench_detail::find_last_if_not(c.begin(), c.end(), pred); result = (it != c.end()) ? int_value(*it) : -1; escape(&result); }
};
template<class C, class Pred, bool Wrap = false>
struct seg_find_last_if_not {
   typedef typename iter_wt<Wrap, typename C::const_iterator>::type cit_t;
   const C &c; Pred pred; int &result;
   seg_find_last_if_not(const C &c_, Pred p_, int &r_) : c(c_), pred(p_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); cit_t it = bc::segmented_find_last_if_not(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), pred); result = (it != iter_w<Wrap>::wrap(c.end())) ? int_value(*it) : -1; escape(&result); }
};

// --- equal ---
template<class C, class R2>
struct std_equal {
   const C &c; const R2 &range2; int &result;
   std_equal(const C &c_, const R2 &r2_, int &r_) : c(c_), range2(r2_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); result = std::equal(c.begin(), c.end(), range2.begin()) ? 1 : 0; escape(&result); }
};
template<class C, class R2, bool Wrap = false>
struct seg_equal {
   const C &c; const R2 &range2; int &result;
   seg_equal(const C &c_, const R2 &r2_, int &r_) : c(c_), range2(r2_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); result = bc::segmented_equal(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), iter_w<Wrap>::wrap(range2.begin())) ? 1 : 0; escape(&result); }
};

// --- replace ---
template<class C>
struct std_replace_op {
   typedef const typename C::value_type* cptr;
   C &c2; cptr &pold_val; cptr &pnew_val;
   std_replace_op(C &c_, cptr &po_, cptr &pn_) : c2(c_), pold_val(po_), pnew_val(pn_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); std::replace(c2.begin(), c2.end(), *pold_val, *pnew_val); escape(&c2); cptr pt(pold_val); pold_val = pnew_val; pnew_val = pt; }
};
template<class C, bool Wrap = false>
struct seg_replace_op {
   typedef const typename C::value_type* cptr;
   C &c2; cptr &pold_val; cptr &pnew_val;
   seg_replace_op(C &c_, cptr &po_, cptr &pn_) : c2(c_), pold_val(po_), pnew_val(pn_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); bc::segmented_replace(iter_w<Wrap>::wrap(c2.begin()), iter_w<Wrap>::wrap(c2.end()), *pold_val, *pnew_val); escape(&c2); cptr pt(pold_val); pold_val = pnew_val; pnew_val = pt; }
};

// --- transform ---
template<class C>
struct std_transform {
   typedef typename C::value_type VT;
   const C &c; boost::container::vector<VT> &out;
   std_transform(const C &c_, boost::container::vector<VT> &o_) : c(c_), out(o_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); std::transform(c.begin(), c.end(), out.begin(), add_one<VT>()); escape(&*out.begin()); }
};
template<class C, bool Wrap = false>
struct seg_transform {
   typedef typename C::value_type VT;
   const C &c; boost::container::vector<VT> &out;
   seg_transform(const C &c_, boost::container::vector<VT> &o_) : c(c_), out(o_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); bc::segmented_transform(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), iter_w<Wrap>::wrap(out.begin()), add_one<VT>()); escape(&*out.begin()); }
};

// --- fill_n ---
template<class C>
struct std_fill_n {
   C &c2; typename C::difference_type n; const typename C::value_type &val;
   std_fill_n(C &c_, typename C::difference_type n_, const typename C::value_type &v_) : c2(c_), n(n_), val(v_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); std::fill_n(c2.begin(), n, val); escape(&c2); }
};
template<class C, bool Wrap = false>
struct seg_fill_n {
   C &c2; typename C::difference_type n; const typename C::value_type &val;
   seg_fill_n(C &c_, typename C::difference_type n_, const typename C::value_type &v_) : c2(c_), n(n_), val(v_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); bc::segmented_fill_n(iter_w<Wrap>::wrap(c2.begin()), n, val); escape(&c2); }
};

// --- copy_n ---
template<class C, class OutT>
struct std_copy_n {
   const C &c; typename C::difference_type n; OutT &out;
   std_copy_n(const C &c_, typename C::difference_type n_, OutT &o_) : c(c_), n(n_), out(o_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); bench_detail::copy_n(c.begin(), n, out.begin()); escape(&*out.begin()); }
};
template<class C, class OutT, bool Wrap = false>
struct seg_copy_n {
   const C &c; typename C::difference_type n; OutT &out;
   seg_copy_n(const C &c_, typename C::difference_type n_, OutT &o_) : c(c_), n(n_), out(o_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); bc::segmented_copy_n(iter_w<Wrap>::wrap(c.begin()), n, iter_w<Wrap>::wrap(out.begin())); escape(&*out.begin()); }
};

// --- generate ---
template<class C>
struct std_generate {
   typedef typename C::value_type VT;
   C &c2; int &result;
   std_generate(C &c_, int &r_) : c2(c_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); std::generate(c2.begin(), c2.end(), counter<VT>()); result = int_value(*c2.begin()); escape(&result); }
};
template<class C, bool Wrap = false>
struct seg_generate {
   typedef typename C::value_type VT;
   C &c2; int &result;
   seg_generate(C &c_, int &r_) : c2(c_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); bc::segmented_generate(iter_w<Wrap>::wrap(c2.begin()), iter_w<Wrap>::wrap(c2.end()), counter<VT>()); result = int_value(*c2.begin()); escape(&result); }
};

// --- generate_n ---
template<class C>
struct std_generate_n {
   typedef typename C::value_type VT;
   C &c2; typename C::difference_type n; int &result;
   std_generate_n(C &c_, typename C::difference_type n_, int &r_) : c2(c_), n(n_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); std::generate_n(c2.begin(), n, counter<VT>()); result = int_value(*c2.begin()); escape(&result); }
};
template<class C, bool Wrap = false>
struct seg_generate_n {
   typedef typename C::value_type VT;
   C &c2; typename C::difference_type n; int &result;
   seg_generate_n(C &c_, typename C::difference_type n_, int &r_) : c2(c_), n(n_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); bc::segmented_generate_n(iter_w<Wrap>::wrap(c2.begin()), n, counter<VT>()); result = int_value(*c2.begin()); escape(&result); }
};

// --- remove_copy ---
template<class C, class OutT>
struct std_remove_copy {
   const C &c; OutT &out; const typename C::value_type &val;
   std_remove_copy(const C &c_, OutT &o_, const typename C::value_type &v_) : c(c_), out(o_), val(v_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); std::remove_copy(c.begin(), c.end(), out.begin(), val); escape(&*out.begin()); }
};
template<class C, class OutT, bool Wrap = false>
struct seg_remove_copy {
   const C &c; OutT &out; const typename C::value_type &val;
   seg_remove_copy(const C &c_, OutT &o_, const typename C::value_type &v_) : c(c_), out(o_), val(v_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); bc::segmented_remove_copy(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), iter_w<Wrap>::wrap(out.begin()), val); escape(&*out.begin()); }
};

// --- remove_copy_if ---
template<class C, class OutT, class Pred>
struct std_remove_copy_if {
   const C &c; OutT &out; Pred pred;
   std_remove_copy_if(const C &c_, OutT &o_, Pred p_) : c(c_), out(o_), pred(p_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); std::remove_copy_if(c.begin(), c.end(), out.begin(), pred); escape(&*out.begin()); }
};
template<class C, class OutT, class Pred, bool Wrap = false>
struct seg_remove_copy_if {
   const C &c; OutT &out; Pred pred;
   seg_remove_copy_if(const C &c_, OutT &o_, Pred p_) : c(c_), out(o_), pred(p_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); bc::segmented_remove_copy_if(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), iter_w<Wrap>::wrap(out.begin()), pred); escape(&*out.begin()); }
};

// --- reverse ---
template<class C>
struct std_reverse {
   C &c2; int &result;
   std_reverse(C &c_, int &r_) : c2(c_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); std::reverse(c2.begin(), c2.end()); result = int_value(*c2.begin()); escape(&result); }
};
template<class C, bool Wrap = false>
struct seg_reverse {
   C &c2; int &result;
   seg_reverse(C &c_, int &r_) : c2(c_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); bc::segmented_reverse(iter_w<Wrap>::wrap(c2.begin()), iter_w<Wrap>::wrap(c2.end())); result = int_value(*c2.begin()); escape(&result); }
};

// --- reverse_copy ---
template<class C, class OutT>
struct std_reverse_copy {
   const C &c; OutT &out;
   std_reverse_copy(const C &c_, OutT &o_) : c(c_), out(o_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); std::reverse_copy(c.begin(), c.end(), out.begin()); escape(&*out.begin()); }
};
template<class C, class OutT, bool Wrap = false>
struct seg_reverse_copy {
   const C &c; OutT &out;
   seg_reverse_copy(const C &c_, OutT &o_) : c(c_), out(o_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); bc::segmented_reverse_copy(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), iter_w<Wrap>::wrap(out.begin())); escape(&*out.begin()); }
};

// --- is_sorted ---
template<class C>
struct std_is_sorted {
   const C &c; int &result;
   std_is_sorted(const C &c_, int &r_) : c(c_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); result = bench_detail::is_sorted(c.begin(), c.end()) ? 1 : 0; escape(&result); }
};
template<class C, bool Wrap = false>
struct seg_is_sorted {
   const C &c; int &result;
   seg_is_sorted(const C &c_, int &r_) : c(c_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); result = bc::segmented_is_sorted(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end())) ? 1 : 0; escape(&result); }
};

// --- is_sorted_until ---
template<class C>
struct std_is_sorted_until {
   typedef typename C::const_iterator cit_t;
   const C &c; int &result;
   std_is_sorted_until(const C &c_, int &r_) : c(c_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); cit_t it = bench_detail::is_sorted_until(c.begin(), c.end()); result = (it == c.end()) ? 1 : 0; escape(&result); }
};
template<class C, bool Wrap = false>
struct seg_is_sorted_until {
   typedef typename iter_wt<Wrap, typename C::const_iterator>::type cit_t;
   const C &c; int &result;
   seg_is_sorted_until(const C &c_, int &r_) : c(c_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); cit_t it = bc::segmented_is_sorted_until(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end())); result = (it == iter_w<Wrap>::wrap(c.end())) ? 1 : 0; escape(&result); }
};

// --- is_partitioned ---
template<class C, class Pred>
struct std_is_partitioned {
   const C &c; Pred pred; int &result;
   std_is_partitioned(const C &c_, Pred p_, int &r_) : c(c_), pred(p_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); result = bench_detail::is_partitioned(c.begin(), c.end(), pred) ? 1 : 0; escape(&result); }
};
template<class C, class Pred, bool Wrap = false>
struct seg_is_partitioned {
   const C &c; Pred pred; int &result;
   seg_is_partitioned(const C &c_, Pred p_, int &r_) : c(c_), pred(p_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); result = bc::segmented_is_partitioned(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), pred) ? 1 : 0; escape(&result); }
};

// --- merge ---
template<class C1, class C2, class OutT>
struct std_merge {
   const C1 &c; const C2 &c2; OutT &out;
   std_merge(const C1 &c_, const C2 &c2_, OutT &o_) : c(c_), c2(c2_), out(o_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); std::merge(c.begin(), c.end(), c2.begin(), c2.end(), out.begin()); escape(&*out.begin()); }
};
template<class C1, class C2, class OutT, bool Wrap = false>
struct seg_merge {
   const C1 &c; const C2 &c2; OutT &out;
   seg_merge(const C1 &c_, const C2 &c2_, OutT &o_) : c(c_), c2(c2_), out(o_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); bc::segmented_merge(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), iter_w<Wrap>::wrap(c2.begin()), iter_w<Wrap>::wrap(c2.end()), iter_w<Wrap>::wrap(out.begin())); escape(&*out.begin()); }
};

// --- mismatch ---
template<class C, class R2>
struct std_mismatch {
   const C &c; const R2 &range2; int &result;
   std_mismatch(const C &c_, const R2 &r2_, int &r_) : c(c_), range2(r2_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); result = (std::mismatch(c.begin(), c.end(), range2.begin()).first == c.end()) ? 1 : 0; escape(&result); }
};
template<class C, class R2, bool Wrap = false>
struct seg_mismatch {
   const C &c; const R2 &range2; int &result;
   seg_mismatch(const C &c_, const R2 &r2_, int &r_) : c(c_), range2(r2_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); result = (bc::segmented_mismatch(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), iter_w<Wrap>::wrap(range2.begin())).first == iter_w<Wrap>::wrap(c.end())) ? 1 : 0; escape(&result); }
};

// --- mismatch (two-range) ---
template<class C, class R2>
struct std_mismatch_2r {
   const C &c; const R2 &range2; int &result;
   std_mismatch_2r(const C &c_, const R2 &r2_, int &r_) : c(c_), range2(r2_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); result = (bench_detail::mismatch(c.begin(), c.end(), range2.begin(), range2.end()).first == c.end()) ? 1 : 0; escape(&result); }
};
template<class C, class R2, bool Wrap = false>
struct seg_mismatch_2r {
   const C &c; const R2 &range2; int &result;
   seg_mismatch_2r(const C &c_, const R2 &r2_, int &r_) : c(c_), range2(r2_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); result = (bc::segmented_mismatch(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), iter_w<Wrap>::wrap(range2.begin()), iter_w<Wrap>::wrap(range2.end())).first == iter_w<Wrap>::wrap(c.end())) ? 1 : 0; escape(&result); }
};

// --- swap_ranges ---
template<class C>
struct std_swap_ranges {
   C &c2; C &c3; int &result;
   std_swap_ranges(C &c2_, C &c3_, int &r_) : c2(c2_), c3(c3_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); std::swap_ranges(c2.begin(), c2.end(), c3.begin()); result = int_value(*c2.begin()); escape(&result); }
};
template<class C, bool Wrap = false>
struct seg_swap_ranges {
   C &c2; C &c3; int &result;
   seg_swap_ranges(C &c2_, C &c3_, int &r_) : c2(c2_), c3(c3_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); bc::segmented_swap_ranges(iter_w<Wrap>::wrap(c2.begin()), iter_w<Wrap>::wrap(c2.end()), iter_w<Wrap>::wrap(c3.begin())); result = int_value(*c2.begin()); escape(&result); }
};

// --- search ---
template<class C>
struct std_search {
   typedef typename C::const_iterator cit_t;
   typedef typename C::value_type VT;
   const C &c; const VT *pattern; std::size_t pat_size; int &result;
   std_search(const C &c_, const VT *p_, std::size_t ps_, int &r_) : c(c_), pattern(p_), pat_size(ps_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); cit_t it = std::search(c.begin(), c.end(), pattern, pattern + pat_size); result = (it == c.end()) ? 0 : 1; escape(&result); }
};
template<class C, bool Wrap = false>
struct seg_search {
   typedef typename iter_wt<Wrap, typename C::const_iterator>::type cit_t;
   typedef typename C::value_type VT;
   const C &c; const VT *pattern; std::size_t pat_size; int &result;
   seg_search(const C &c_, const VT *p_, std::size_t ps_, int &r_) : c(c_), pattern(p_), pat_size(ps_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); cit_t it = bc::segmented_search(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), pattern, pattern + pat_size); result = (it == iter_w<Wrap>::wrap(c.end())) ? 0 : 1; escape(&result); }
};

// --- search_n ---
template<class C>
struct std_search_n {
   typedef typename C::const_iterator cit_t;
   const C &c; typename C::difference_type cnt; const typename C::value_type &val; int &result;
   std_search_n(const C &c_, typename C::difference_type n_, const typename C::value_type &v_, int &r_) : c(c_), cnt(n_), val(v_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); cit_t it = std::search_n(c.begin(), c.end(), cnt, val); result = (it == c.end()) ? 0 : 1; escape(&result); }
};
template<class C, bool Wrap = false>
struct seg_search_n {
   typedef typename iter_wt<Wrap, typename C::const_iterator>::type cit_t;
   const C &c; typename C::difference_type cnt; const typename C::value_type &val; int &result;
   seg_search_n(const C &c_, typename C::difference_type n_, const typename C::value_type &v_, int &r_) : c(c_), cnt(n_), val(v_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); cit_t it = bc::segmented_search_n(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), cnt, val); result = (it == iter_w<Wrap>::wrap(c.end())) ? 0 : 1; escape(&result); }
};

// --- set_union ---
template<class C1, class C2, class OutT>
struct std_set_union {
   const C1 &c; const C2 &c2; OutT &out;
   std_set_union(const C1 &c_, const C2 &c2_, OutT &o_) : c(c_), c2(c2_), out(o_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); std::set_union(c.begin(), c.end(), c2.begin(), c2.end(), out.begin()); escape(&*out.begin()); }
};
template<class C1, class C2, class OutT, bool Wrap = false>
struct seg_set_union {
   const C1 &c; const C2 &c2; OutT &out;
   seg_set_union(const C1 &c_, const C2 &c2_, OutT &o_) : c(c_), c2(c2_), out(o_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); bc::segmented_set_union(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), iter_w<Wrap>::wrap(c2.begin()), iter_w<Wrap>::wrap(c2.end()), iter_w<Wrap>::wrap(out.begin())); escape(&*out.begin()); }
};

// --- set_difference ---
template<class C1, class C2, class OutT>
struct std_set_difference {
   const C1 &c; const C2 &c2; OutT &out;
   std_set_difference(const C1 &c_, const C2 &c2_, OutT &o_) : c(c_), c2(c2_), out(o_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); std::set_difference(c.begin(), c.end(), c2.begin(), c2.end(), out.begin()); escape(&*out.begin()); }
};
template<class C1, class C2, class OutT, bool Wrap = false>
struct seg_set_difference {
   const C1 &c; const C2 &c2; OutT &out;
   seg_set_difference(const C1 &c_, const C2 &c2_, OutT &o_) : c(c_), c2(c2_), out(o_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); bc::segmented_set_difference(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), iter_w<Wrap>::wrap(c2.begin()), iter_w<Wrap>::wrap(c2.end()), iter_w<Wrap>::wrap(out.begin())); escape(&*out.begin()); }
};

// --- set_intersection ---
template<class C1, class C2, class OutT>
struct std_set_intersection {
   const C1 &c; const C2 &c2; OutT &out;
   std_set_intersection(const C1 &c_, const C2 &c2_, OutT &o_) : c(c_), c2(c2_), out(o_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); std::set_intersection(c.begin(), c.end(), c2.begin(), c2.end(), out.begin()); escape(&*out.begin()); }
};
template<class C1, class C2, class OutT, bool Wrap = false>
struct seg_set_intersection {
   const C1 &c; const C2 &c2; OutT &out;
   seg_set_intersection(const C1 &c_, const C2 &c2_, OutT &o_) : c(c_), c2(c2_), out(o_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); bc::segmented_set_intersection(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), iter_w<Wrap>::wrap(c2.begin()), iter_w<Wrap>::wrap(c2.end()), iter_w<Wrap>::wrap(out.begin())); escape(&*out.begin()); }
};

// --- set_symmetric_difference ---
template<class C1, class C2, class OutT>
struct std_set_symmetric_difference {
   const C1 &c; const C2 &c2; OutT &out;
   std_set_symmetric_difference(const C1 &c_, const C2 &c2_, OutT &o_) : c(c_), c2(c2_), out(o_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); std::set_symmetric_difference(c.begin(), c.end(), c2.begin(), c2.end(), out.begin()); escape(&*out.begin()); }
};
template<class C1, class C2, class OutT, bool Wrap = false>
struct seg_set_symmetric_difference {
   const C1 &c; const C2 &c2; OutT &out;
   seg_set_symmetric_difference(const C1 &c_, const C2 &c2_, OutT &o_) : c(c_), c2(c2_), out(o_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); bc::segmented_set_symmetric_difference(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), iter_w<Wrap>::wrap(c2.begin()), iter_w<Wrap>::wrap(c2.end()), iter_w<Wrap>::wrap(out.begin())); escape(&*out.begin()); }
};

// --- partition_copy ---
template<class C>
struct std_partition_copy {
   typedef typename C::value_type VT;
   const C &c; boost::container::vector<VT> &t_out; boost::container::vector<VT> &f_out;
   std_partition_copy(const C &c_, boost::container::vector<VT> &t_, boost::container::vector<VT> &f_) : c(c_), t_out(t_), f_out(f_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); bench_detail::partition_copy(c.begin(), c.end(), t_out.begin(), f_out.begin(), is_odd<VT>()); escape(&t_out[0]); }
};
template<class C, bool Wrap = false>
struct seg_partition_copy {
   typedef typename C::value_type VT;
   const C &c; boost::container::vector<VT> &t_out; boost::container::vector<VT> &f_out;
   seg_partition_copy(const C &c_, boost::container::vector<VT> &t_, boost::container::vector<VT> &f_) : c(c_), t_out(t_), f_out(f_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); bc::segmented_partition_copy(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), iter_w<Wrap>::wrap(t_out.begin()), iter_w<Wrap>::wrap(f_out.begin()), is_odd<VT>()); escape(&t_out[0]); }
};

// --- partition_point ---
template<class C, class Pred>
struct std_partition_point {
   typedef typename C::const_iterator cit_t;
   const C &c; Pred pred; int &result;
   std_partition_point(const C &c_, Pred p_, int &r_) : c(c_), pred(p_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); cit_t it = bench_detail::partition_point(c.begin(), c.end(), pred); result = (it == c.end()) ? 1 : 0; escape(&result); }
};
template<class C, class Pred, bool Wrap = false>
struct seg_partition_point {
   typedef typename iter_wt<Wrap, typename C::const_iterator>::type cit_t;
   const C &c; Pred pred; int &result;
   seg_partition_point(const C &c_, Pred p_, int &r_) : c(c_), pred(p_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); cit_t it = bc::segmented_partition_point(iter_w<Wrap>::wrap(c.begin()), iter_w<Wrap>::wrap(c.end()), pred); result = (it == iter_w<Wrap>::wrap(c.end())) ? 1 : 0; escape(&result); }
};

// --- Batch: replace_if ---
template<class C, class Pred>
struct std_replace_if_batch {
   batch_state<C> &bs; Pred pred; const typename C::value_type &new_val;
   std_replace_if_batch(batch_state<C> &b_, Pred p_, const typename C::value_type &nv_) : bs(b_), pred(p_), new_val(nv_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); std::replace_if(bs.cs[bs.idx]->begin(), bs.cs[bs.idx]->end(), pred, new_val); escape(bs.cs[bs.idx]); ++bs.idx; }
};
template<class C, class Pred, bool Wrap = false>
struct seg_replace_if_batch {
   batch_state<C> &bs; Pred pred; const typename C::value_type &new_val;
   seg_replace_if_batch(batch_state<C> &b_, Pred p_, const typename C::value_type &nv_) : bs(b_), pred(p_), new_val(nv_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); bc::segmented_replace_if(iter_w<Wrap>::wrap(bs.cs[bs.idx]->begin()), iter_w<Wrap>::wrap(bs.cs[bs.idx]->end()), pred, new_val); escape(bs.cs[bs.idx]); ++bs.idx; }
};

// --- Batch: remove ---
template<class C>
struct std_remove_batch {
   typedef typename C::iterator it_t;
   batch_state<C> &bs; const typename C::value_type &val; int &result;
   std_remove_batch(batch_state<C> &b_, const typename C::value_type &v_, int &r_) : bs(b_), val(v_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); it_t it = std::remove(bs.cs[bs.idx]->begin(), bs.cs[bs.idx]->end(), val); result = (it == bs.cs[bs.idx]->end()) ? 1 : 0; escape(&result); ++bs.idx; }
};
template<class C, bool Wrap = false>
struct seg_remove_batch {
   typedef typename iter_wt<Wrap, typename C::iterator>::type it_t;
   batch_state<C> &bs; const typename C::value_type &val; int &result;
   seg_remove_batch(batch_state<C> &b_, const typename C::value_type &v_, int &r_) : bs(b_), val(v_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); it_t it = bc::segmented_remove(iter_w<Wrap>::wrap(bs.cs[bs.idx]->begin()), iter_w<Wrap>::wrap(bs.cs[bs.idx]->end()), val); result = (it == iter_w<Wrap>::wrap(bs.cs[bs.idx]->end())) ? 1 : 0; escape(&result); ++bs.idx; }
};

// --- Batch: remove_if ---
template<class C, class Pred>
struct std_remove_if_batch {
   typedef typename C::iterator it_t;
   batch_state<C> &bs; Pred pred; int &result;
   std_remove_if_batch(batch_state<C> &b_, Pred p_, int &r_) : bs(b_), pred(p_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); it_t it = std::remove_if(bs.cs[bs.idx]->begin(), bs.cs[bs.idx]->end(), pred); result = (it == bs.cs[bs.idx]->end()) ? 1 : 0; escape(&result); ++bs.idx; }
};
template<class C, class Pred, bool Wrap = false>
struct seg_remove_if_batch {
   typedef typename iter_wt<Wrap, typename C::iterator>::type it_t;
   batch_state<C> &bs; Pred pred; int &result;
   seg_remove_if_batch(batch_state<C> &b_, Pred p_, int &r_) : bs(b_), pred(p_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); it_t it = bc::segmented_remove_if(iter_w<Wrap>::wrap(bs.cs[bs.idx]->begin()), iter_w<Wrap>::wrap(bs.cs[bs.idx]->end()), pred); result = (it == iter_w<Wrap>::wrap(bs.cs[bs.idx]->end())) ? 1 : 0; escape(&result); ++bs.idx; }
};

// --- Batch: partition ---
template<class C, class Pred>
struct std_partition_batch {
   typedef typename C::iterator it_t;
   batch_state<C> &bs; Pred pred; int &result;
   std_partition_batch(batch_state<C> &b_, Pred p_, int &r_) : bs(b_), pred(p_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); it_t it = std::partition(bs.cs[bs.idx]->begin(), bs.cs[bs.idx]->end(), pred); result = (it == bs.cs[bs.idx]->end()) ? 1 : 0; escape(&result); ++bs.idx; }
};
template<class C, class Pred, bool Wrap = false>
struct seg_partition_batch {
   typedef typename iter_wt<Wrap, typename C::iterator>::type it_t;
   batch_state<C> &bs; Pred pred; int &result;
   seg_partition_batch(batch_state<C> &b_, Pred p_, int &r_) : bs(b_), pred(p_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); it_t it = bc::segmented_partition(iter_w<Wrap>::wrap(bs.cs[bs.idx]->begin()), iter_w<Wrap>::wrap(bs.cs[bs.idx]->end()), pred); result = (it == iter_w<Wrap>::wrap(bs.cs[bs.idx]->end())) ? 1 : 0; escape(&result); ++bs.idx; }
};

// --- Batch: stable_partition ---
template<class C, class Pred>
struct std_stable_partition_batch {
   typedef typename C::iterator it_t;
   batch_state<C> &bs; Pred pred; int &result;
   std_stable_partition_batch(batch_state<C> &b_, Pred p_, int &r_) : bs(b_), pred(p_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); it_t it = std::stable_partition(bs.cs[bs.idx]->begin(), bs.cs[bs.idx]->end(), pred); result = (it == bs.cs[bs.idx]->end()) ? 1 : 0; escape(&result); ++bs.idx; }
};
template<class C, class Pred, bool Wrap = false>
struct seg_stable_partition_batch {
   typedef typename iter_wt<Wrap, typename C::iterator>::type it_t;
   batch_state<C> &bs; Pred pred; int &result;
   seg_stable_partition_batch(batch_state<C> &b_, Pred p_, int &r_) : bs(b_), pred(p_), result(r_) {}
   BOOST_CONTAINER_FORCEINLINE void operator()()
   { clobber(); it_t it = bc::segmented_stable_partition(iter_w<Wrap>::wrap(bs.cs[bs.idx]->begin()), iter_w<Wrap>::wrap(bs.cs[bs.idx]->end()), pred); result = (it == iter_w<Wrap>::wrap(bs.cs[bs.idx]->end())) ? 1 : 0; escape(&result); ++bs.idx; }
};

} // namespace bench_ops

//////////////////////////////////////////////////////////////////////////////
// Individual benchmarks
//////////////////////////////////////////////////////////////////////////////

template<class C, class Pred>
void bench_all_of(const C &c, std::size_t iters, const char* cname,
                  Pred pred, const char* label)
{
   int result = 0;
   compare_batch(iters, c.size(),
      bench_ops::std_all_of<C, Pred>(c, pred, result),
      bench_ops::seg_all_of<C, Pred>(c, pred, result),
      bench_ops::seg_all_of<C, Pred, true>(c, pred, result), label, cname);
}

template<class C, class Pred>
void bench_any_of(const C &c, std::size_t iters, const char* cname,
                  Pred pred, const char* label)
{
   int result = 0;
   compare_batch(iters, c.size(),
      bench_ops::std_any_of<C, Pred>(c, pred, result),
      bench_ops::seg_any_of<C, Pred>(c, pred, result),
      bench_ops::seg_any_of<C, Pred, true>(c, pred, result), label, cname);
}

template<class C, class Pred>
void bench_none_of(const C &c, std::size_t iters, const char* cname,
                   Pred pred, const char* label)
{
   int result = 0;
   compare_batch(iters, c.size(),
      bench_ops::std_none_of<C, Pred>(c, pred, result),
      bench_ops::seg_none_of<C, Pred>(c, pred, result),
      bench_ops::seg_none_of<C, Pred, true>(c, pred, result), label, cname);
}

template<class C>
void bench_for_each(const C &c, std::size_t iters, const char* cname)
{
   int result = 0;
   compare_batch(iters, c.size(),
      bench_ops::std_for_each<C>(c, result),
      bench_ops::seg_for_each<C>(c, result),
      bench_ops::seg_for_each<C, true>(c, result), "for_each", cname);
}

template<class InC, class OutC>
void bench_copy(const InC &c, std::size_t iters, const char* cname, const char* label)
{
   OutC out(c.size());
   compare_batch(iters, c.size(),
      bench_ops::std_copy<InC, OutC>(c, out),
      bench_ops::seg_copy<InC, OutC>(c, out),
      bench_ops::seg_copy<InC, OutC, true>(c, out), label, cname);
}

template<class InC, class OutC, class Pred>
void bench_copy_if(const InC &c, std::size_t iters, const char* cname,
                   Pred pred, const char* label)
{
   OutC out(c.size());
   compare_batch(iters, c.size(),
      bench_ops::std_copy_if<InC, OutC, Pred>(c, out, pred),
      bench_ops::seg_copy_if<InC, OutC, Pred>(c, out, pred),
      bench_ops::seg_copy_if<InC, OutC, Pred, true>(c, out, pred), label, cname);
}

template<class C>
void bench_fill(const C &c, std::size_t iters, const char* cname)
{
   typedef typename C::value_type VT;
   VT val(42);
   C c2(c);
   compare_batch(iters, c.size(),
      bench_ops::std_fill<C>(c2, val),
      bench_ops::seg_fill<C>(c2, val),
      bench_ops::seg_fill<C, true>(c2, val), "fill", cname);
}

template<class C>
void bench_count(const C &c, std::size_t iters, const char* cname,
                 const typename C::value_type& val, const char* label)
{
   int result = 0;
   compare_batch(iters, c.size(),
      bench_ops::std_count<C>(c, val, result),
      bench_ops::seg_count<C>(c, val, result),
      bench_ops::seg_count<C, true>(c, val, result), label, cname);
}

template<class C, class Pred>
void bench_count_if(const C &c, std::size_t iters, const char* cname,
                    Pred pred, const char* label)
{
   int result = 0;
   compare_batch(iters, c.size(),
      bench_ops::std_count_if<C, Pred>(c, pred, result),
      bench_ops::seg_count_if<C, Pred>(c, pred, result),
      bench_ops::seg_count_if<C, Pred, true>(c, pred, result), label, cname);
}

template<class C>
void bench_find(const C &c, std::size_t iters, const char* cname,
                const typename C::value_type& val, const char* label)
{
   int result = 0;
   compare_batch(iters, c.size(),
      bench_ops::std_find<C>(c, val, result),
      bench_ops::seg_find<C>(c, val, result),
      bench_ops::seg_find<C, true>(c, val, result), label, cname);
}

template<class C, class Pred>
void bench_find_if(const C &c, std::size_t iters, const char* cname,
                   Pred pred, const char* label)
{
   int result = 0;
   compare_batch(iters, c.size(),
      bench_ops::std_find_if<C, Pred>(c, pred, result),
      bench_ops::seg_find_if<C, Pred>(c, pred, result),
      bench_ops::seg_find_if<C, Pred, true>(c, pred, result), label, cname);
}

template<class C, class Pred>
void bench_find_if_not(const C &c, std::size_t iters, const char* cname,
                       Pred pred, const char* label)
{
   int result = 0;
   compare_batch(iters, c.size(),
      bench_ops::std_find_if_not<C, Pred>(c, pred, result),
      bench_ops::seg_find_if_not<C, Pred>(c, pred, result),
      bench_ops::seg_find_if_not<C, Pred, true>(c, pred, result), label, cname);
}

template<class C>
void bench_find_last(const C &c, std::size_t iters, const char* cname,
                     const typename C::value_type& val, const char* label)
{
   int result = 0;
   compare_batch(iters, c.size(),
      bench_ops::std_find_last<C>(c, val, result),
      bench_ops::seg_find_last<C>(c, val, result),
      bench_ops::seg_find_last<C, true>(c, val, result), label, cname);
}

template<class C, class Pred>
void bench_find_last_if(const C &c, std::size_t iters, const char* cname,
                        Pred pred, const char* label)
{
   int result = 0;
   compare_batch(iters, c.size(),
      bench_ops::std_find_last_if<C, Pred>(c, pred, result),
      bench_ops::seg_find_last_if<C, Pred>(c, pred, result),
      bench_ops::seg_find_last_if<C, Pred, true>(c, pred, result), label, cname);
}

template<class C, class Pred>
void bench_find_last_if_not(const C &c, std::size_t iters, const char* cname,
                            Pred pred, const char* label)
{
   int result = 0;
   compare_batch(iters, c.size(),
      bench_ops::std_find_last_if_not<C, Pred>(c, pred, result),
      bench_ops::seg_find_last_if_not<C, Pred>(c, pred, result),
      bench_ops::seg_find_last_if_not<C, Pred, true>(c, pred, result), label, cname);
}

template<class InC1, class InC2>
void bench_equal(const InC1 &c, const InC2 &c2, std::size_t iters, const char* cname,
                 const char* label)
{
   int result = 0;
   compare_batch(iters, c.size(),
      bench_ops::std_equal<InC1, InC2>(c, c2, result),
      bench_ops::seg_equal<InC1, InC2>(c, c2, result),
      bench_ops::seg_equal<InC1, InC2, true>(c, c2, result), label, cname);
}

template<class C>
void bench_replace(const C &c, std::size_t iters, const char* cname,
                   const typename C::value_type& old_val,
                   const typename C::value_type& new_val, const char* label)
{
   typedef const typename C::value_type* cptr;
   cptr p1o = &old_val, p1n = &new_val;
   cptr p2o = &old_val, p2n = &new_val;
   cptr p3o = &old_val, p3n = &new_val;
   C c2a(c), c2b(c), c2c(c);
   compare_batch(iters, c.size(),
      bench_ops::std_replace_op<C>(c2a, p1o, p1n),
      bench_ops::seg_replace_op<C>(c2b, p2o, p2n),
      bench_ops::seg_replace_op<C, true>(c2c, p3o, p3n), label, cname);
}

template<class C, class Pred>
void bench_replace_if(const C &c, std::size_t iters, const char* cname,
                      Pred pred, const typename C::value_type& new_val,
                      const char* label)
{
   bench_ops::batch_state<C> bs1(c), bs2(c), bs3(c);
   compare_batch(iters, c.size(),
      bench_ops::std_replace_if_batch<C, Pred>(bs1, pred, new_val), bench_ops::batch_reset<C>(bs1),
      bench_ops::seg_replace_if_batch<C, Pred>(bs2, pred, new_val), bench_ops::batch_reset<C>(bs2),
      bench_ops::seg_replace_if_batch<C, Pred, true>(bs3, pred, new_val), bench_ops::batch_reset<C>(bs3),
      label, cname);
}

template<class C>
void bench_transform(const C &c, std::size_t iters, const char* cname)
{
   typedef typename C::value_type VT;
   boost::container::vector<VT> out(c.size());
   compare_batch(iters, c.size(),
      bench_ops::std_transform<C>(c, out),
      bench_ops::seg_transform<C>(c, out),
      bench_ops::seg_transform<C, true>(c, out), "transform", cname);
}

template<class C>
void bench_fill_n(const C &c, std::size_t iters, const char* cname)
{
   typedef typename C::value_type VT;
   VT val(42);
   typename C::difference_type n =
      static_cast<typename C::difference_type>(c.size());
   C c2(c);
   compare_batch(iters, c.size(),
      bench_ops::std_fill_n<C>(c2, n, val),
      bench_ops::seg_fill_n<C>(c2, n, val),
      bench_ops::seg_fill_n<C, true>(c2, n, val), "fill_n", cname);
}

template<class InC, class OutC>
void bench_copy_n(const InC &c, std::size_t iters, const char* cname, const char* label)
{
   OutC out(c.size());
   typename InC::difference_type n =
      static_cast<typename InC::difference_type>(c.size());
   compare_batch(iters, c.size(),
      bench_ops::std_copy_n<InC, OutC>(c, n, out),
      bench_ops::seg_copy_n<InC, OutC>(c, n, out),
      bench_ops::seg_copy_n<InC, OutC, true>(c, n, out), label, cname);
}

template<class C>
void bench_generate(const C &c, std::size_t iters, const char* cname)
{
   C c2(c);
   int result = 0;
   compare_batch(iters, c.size(),
      bench_ops::std_generate<C>(c2, result),
      bench_ops::seg_generate<C>(c2, result),
      bench_ops::seg_generate<C, true>(c2, result), "generate", cname);
}

template<class C>
void bench_generate_n(const C &c, std::size_t iters, const char* cname)
{
   typename C::difference_type n =
      static_cast<typename C::difference_type>(c.size());
   C c2(c);
   int result = 0;
   compare_batch(iters, c.size(),
      bench_ops::std_generate_n<C>(c2, n, result),
      bench_ops::seg_generate_n<C>(c2, n, result),
      bench_ops::seg_generate_n<C, true>(c2, n, result), "generate_n", cname);
}

template<class C>
void bench_remove(const C &c, std::size_t iters, const char* cname,
                  const typename C::value_type& val, const char* label)
{
   int result = 0;
   bench_ops::batch_state<C> bs1(c), bs2(c), bs3(c);
   compare_batch(iters, c.size(),
      bench_ops::std_remove_batch<C>(bs1, val, result), bench_ops::batch_reset<C>(bs1),
      bench_ops::seg_remove_batch<C>(bs2, val, result), bench_ops::batch_reset<C>(bs2),
      bench_ops::seg_remove_batch<C, true>(bs3, val, result), bench_ops::batch_reset<C>(bs3),
      label, cname);
}

template<class C, class Pred>
void bench_remove_if(const C &c, std::size_t iters, const char* cname,
                     Pred pred, const char* label)
{
   int result = 0;
   bench_ops::batch_state<C> bs1(c), bs2(c), bs3(c);
   compare_batch(iters, c.size(),
      bench_ops::std_remove_if_batch<C, Pred>(bs1, pred, result), bench_ops::batch_reset<C>(bs1),
      bench_ops::seg_remove_if_batch<C, Pred>(bs2, pred, result), bench_ops::batch_reset<C>(bs2),
      bench_ops::seg_remove_if_batch<C, Pred, true>(bs3, pred, result), bench_ops::batch_reset<C>(bs3),
      label, cname);
}

template<class InC, class OutC>
void bench_remove_copy(const InC &c, std::size_t iters, const char* cname,
                       const typename InC::value_type& val, const char* label)
{
   OutC out(c.size());
   compare_batch(iters, c.size(),
      bench_ops::std_remove_copy<InC, OutC>(c, out, val),
      bench_ops::seg_remove_copy<InC, OutC>(c, out, val),
      bench_ops::seg_remove_copy<InC, OutC, true>(c, out, val), label, cname);
}

template<class InC, class OutC, class Pred>
void bench_remove_copy_if(const InC &c, std::size_t iters, const char* cname,
                          Pred pred, const char* label)
{
   OutC out(c.size());
   compare_batch(iters, c.size(),
      bench_ops::std_remove_copy_if<InC, OutC, Pred>(c, out, pred),
      bench_ops::seg_remove_copy_if<InC, OutC, Pred>(c, out, pred),
      bench_ops::seg_remove_copy_if<InC, OutC, Pred, true>(c, out, pred), label, cname);
}

template<class C>
void bench_reverse(const C &c, std::size_t iters, const char* cname)
{
   C c2(c);
   int result = 0;
   compare_batch(iters, c.size(),
      bench_ops::std_reverse<C>(c2, result),
      bench_ops::seg_reverse<C>(c2, result),
      bench_ops::seg_reverse<C, true>(c2, result), "reverse", cname);
}

template<class InC, class OutC>
void bench_reverse_copy(const InC &c, std::size_t iters, const char* cname, const char* label)
{
   OutC out(c.size());
   compare_batch(iters, c.size(),
      bench_ops::std_reverse_copy<InC, OutC>(c, out),
      bench_ops::seg_reverse_copy<InC, OutC>(c, out),
      bench_ops::seg_reverse_copy<InC, OutC, true>(c, out), label, cname);
}

template<class C>
void bench_is_sorted(const C &c, std::size_t iters, const char* cname,
                     const char* label)
{
   int result = 0;
   compare_batch(iters, c.size(),
      bench_ops::std_is_sorted<C>(c, result),
      bench_ops::seg_is_sorted<C>(c, result),
      bench_ops::seg_is_sorted<C, true>(c, result), label, cname);
}

template<class C>
void bench_is_sorted_until(const C &c, std::size_t iters, const char* cname,
                           const char* label)
{
   int result = 0;
   compare_batch(iters, c.size(),
      bench_ops::std_is_sorted_until<C>(c, result),
      bench_ops::seg_is_sorted_until<C>(c, result),
      bench_ops::seg_is_sorted_until<C, true>(c, result), label, cname);
}

template<class C, class Pred>
void bench_is_partitioned(const C &c, std::size_t iters, const char* cname,
                          Pred pred, const char* label)
{
   int result = 0;
   compare_batch(iters, c.size(),
      bench_ops::std_is_partitioned<C, Pred>(c, pred, result),
      bench_ops::seg_is_partitioned<C, Pred>(c, pred, result),
      bench_ops::seg_is_partitioned<C, Pred, true>(c, pred, result), label, cname);
}

template<class C1, class C2, class OutT>
void bench_merge(const C1 &c1, const C2 &c2, std::size_t iters,
                 const char* cname, const char* label)
{
   OutT out(c1.size() + c2.size());
   compare_batch(iters, c1.size(),
      bench_ops::std_merge<C1, C2, OutT>(c1, c2, out),
      bench_ops::seg_merge<C1, C2, OutT>(c1, c2, out),
      bench_ops::seg_merge<C1, C2, OutT, true>(c1, c2, out), label, cname);
}

template<class InC1, class InC2>
void bench_mismatch(const InC1 &c, const InC2 &c2, std::size_t iters, const char* cname,
                    const char* label)
{
   int result = 0;
   compare_batch(iters, c.size(),
      bench_ops::std_mismatch<InC1, InC2>(c, c2, result),
      bench_ops::seg_mismatch<InC1, InC2>(c, c2, result),
      bench_ops::seg_mismatch<InC1, InC2, true>(c, c2, result), label, cname);
}

template<class InC1, class InC2>
void bench_mismatch_2r(const InC1 &c, const InC2 &c2, std::size_t iters, const char* cname,
                       const char* label)
{
   int result = 0;
   compare_batch(iters, c.size(),
      bench_ops::std_mismatch_2r<InC1, InC2>(c, c2, result),
      bench_ops::seg_mismatch_2r<InC1, InC2>(c, c2, result),
      bench_ops::seg_mismatch_2r<InC1, InC2, true>(c, c2, result), label, cname);
}

template<class C>
void bench_swap_ranges(const C &c, std::size_t iters, const char* cname)
{
   typedef typename C::iterator it_t;
   typedef typename C::value_type VT;
   C c2(c);
   C c3(c2);
   for (it_t it = c3.begin(), ite = c3.end(); it != ite; ++it)
      *it = VT(int_value(*it) * 3);
   int result = 0;
   compare_batch(iters, c.size(),
      bench_ops::std_swap_ranges<C>(c2, c3, result),
      bench_ops::seg_swap_ranges<C>(c2, c3, result),
      bench_ops::seg_swap_ranges<C, true>(c2, c3, result), "swap_ranges", cname);
}

template<class C>
void bench_search(const C &c, std::size_t iters, const char* cname,
                  const typename C::value_type* pattern, std::size_t pat_size,
                  const char* label)
{
   int result = 0;
   compare_batch(iters, c.size(),
      bench_ops::std_search<C>(c, pattern, pat_size, result),
      bench_ops::seg_search<C>(c, pattern, pat_size, result),
      bench_ops::seg_search<C, true>(c, pattern, pat_size, result), label, cname);
}

template<class C>
void bench_search_n(const C &c, std::size_t iters, const char* cname,
                    typename C::difference_type count,
                    const typename C::value_type& val, const char* label)
{
   int result = 0;
   compare_batch(iters, c.size(),
      bench_ops::std_search_n<C>(c, count, val, result),
      bench_ops::seg_search_n<C>(c, count, val, result),
      bench_ops::seg_search_n<C, true>(c, count, val, result), label, cname);
}

template<class C1, class C2, class OutT>
void bench_set_union(const C1 &c, const C2 &c2, std::size_t iters,
                     const char* cname, const char* label)
{
   OutT out(c.size() + c2.size());
   compare_batch(iters, c.size(),
      bench_ops::std_set_union<C1, C2, OutT>(c, c2, out),
      bench_ops::seg_set_union<C1, C2, OutT>(c, c2, out),
      bench_ops::seg_set_union<C1, C2, OutT, true>(c, c2, out), label, cname);
}

template<class C1, class C2, class OutT>
void bench_set_difference(const C1 &c, const C2 &c2, std::size_t iters,
                           const char* cname, const char* label)
{
   OutT out(c.size());
   compare_batch(iters, c.size(),
      bench_ops::std_set_difference<C1, C2, OutT>(c, c2, out),
      bench_ops::seg_set_difference<C1, C2, OutT>(c, c2, out),
      bench_ops::seg_set_difference<C1, C2, OutT, true>(c, c2, out), label, cname);
}

template<class C1, class C2, class OutT>
void bench_set_intersection(const C1 &c, const C2 &c2, std::size_t iters,
                             const char* cname, const char* label)
{
   OutT out(c.size());
   compare_batch(iters, c.size(),
      bench_ops::std_set_intersection<C1, C2, OutT>(c, c2, out),
      bench_ops::seg_set_intersection<C1, C2, OutT>(c, c2, out),
      bench_ops::seg_set_intersection<C1, C2, OutT, true>(c, c2, out), label, cname);
}

template<class C1, class C2, class OutT>
void bench_set_symmetric_difference(const C1 &c, const C2 &c2, std::size_t iters,
                                     const char* cname, const char* label)
{
   OutT out(c.size() + c2.size());
   compare_batch(iters, c.size(),
      bench_ops::std_set_symmetric_difference<C1, C2, OutT>(c, c2, out),
      bench_ops::seg_set_symmetric_difference<C1, C2, OutT>(c, c2, out),
      bench_ops::seg_set_symmetric_difference<C1, C2, OutT, true>(c, c2, out), label, cname);
}

template<class C, class Pred>
void bench_partition(const C &c, std::size_t iters, const char* cname,
                     Pred pred, const char* label)
{
   int result = 0;
   bench_ops::batch_state<C> bs1(c), bs2(c), bs3(c);
   compare_batch(iters, c.size(),
      bench_ops::std_partition_batch<C, Pred>(bs1, pred, result), bench_ops::batch_reset<C>(bs1),
      bench_ops::seg_partition_batch<C, Pred>(bs2, pred, result), bench_ops::batch_reset<C>(bs2),
      bench_ops::seg_partition_batch<C, Pred, true>(bs3, pred, result), bench_ops::batch_reset<C>(bs3),
      label, cname);
}

template<class C, class Pred>
void bench_stable_partition(const C &c, std::size_t iters, const char* cname,
                            Pred pred, const char* label)
{
   int result = 0;
   bench_ops::batch_state<C> bs1(c), bs2(c), bs3(c);
   compare_batch(iters, c.size(),
      bench_ops::std_stable_partition_batch<C, Pred>(bs1, pred, result), bench_ops::batch_reset<C>(bs1),
      bench_ops::seg_stable_partition_batch<C, Pred>(bs2, pred, result), bench_ops::batch_reset<C>(bs2),
      bench_ops::seg_stable_partition_batch<C, Pred, true>(bs3, pred, result), bench_ops::batch_reset<C>(bs3),
      label, cname);
}

template<class C>
void bench_partition_copy(const C &c, std::size_t iters, const char* cname)
{
   typedef typename C::value_type VT;
   boost::container::vector<VT> t_out(c.size());
   boost::container::vector<VT> f_out(c.size());
   compare_batch(iters, c.size(),
      bench_ops::std_partition_copy<C>(c, t_out, f_out),
      bench_ops::seg_partition_copy<C>(c, t_out, f_out),
      bench_ops::seg_partition_copy<C, true>(c, t_out, f_out), "partition_copy", cname);
}

template<class C, class Pred>
void bench_partition_point(const C &c, std::size_t iters, const char* cname,
                           Pred pred, const char* label)
{
   int result = 0;
   compare_batch(iters, c.size(),
      bench_ops::std_partition_point<C, Pred>(c, pred, result),
      bench_ops::seg_partition_point<C, Pred>(c, pred, result),
      bench_ops::seg_partition_point<C, Pred, true>(c, pred, result), label, cname);
}

//////////////////////////////////////////////////////////////////////////////
// Run all benchmarks for a container type
//////////////////////////////////////////////////////////////////////////////

template<class C>
void run_all(const C& c, std::size_t iters, const char* cname)
{
   typedef typename C::value_type VT;
   typedef boost::container::vector<VT> vec_t;

   const VT zero(0);
   const VT min1(-1);
   const VT quart((int)c.size()/4);
   const VT half((int)c.size()/2);
   const VT threequart((int)c.size()*3/4);

   //Flat (non-segmented) copy of c, used as the first range in the
   //"only-second-range-segmented" (2S) variants of dual-shape algorithms.
   vec_t cv(c.begin(), c.end());

   g_geomean.reset();
   print_subheader();

   //////////////////////////////////////////////////////////////////
   // Group 1: Algorithms with a single range
   //////////////////////////////////////////////////////////////////
#if !defined(BOOST_CONTAINER_BENCH_SEGMENTED_GROUP) || BOOST_CONTAINER_BENCH_SEGMENTED_GROUP == 0 || BOOST_CONTAINER_BENCH_SEGMENTED_GROUP == 10

   //all_of
   bench_all_of(c, iters, cname, is_zero_or_positive<VT>(), "all_of(hit)");
   bench_all_of(c, iters, cname, unequal_to_ref<VT>(half), "all_of(miss)");

   //any_of
   bench_any_of(c, iters, cname, equal_to_ref<VT>(half),   "any_of(hit)");
   bench_any_of(c, iters, cname, is_negative<VT>(), "any_of(miss)");

   //count
   bench_count(c, iters, cname, zero,  "count(hit)");
   bench_count(c, iters, cname, min1, "count(miss)");

   //count_if
   bench_count_if(c, iters, cname, is_odd<VT>(),      "count_if(hit)");
   bench_count_if(c, iters, cname, is_negative<VT>(), "count_if(miss)");

   //fill
   bench_fill(c, iters, cname);

   //fill_n
   bench_fill_n(c, iters, cname);

   //find
   bench_find(c, iters, cname, half, "find(hit)");
   bench_find(c, iters, cname, min1, "find(miss)");

   //find_if
   bench_find_if(c, iters, cname, equal_to_ref<VT>(half), "find_if(hit)");
   bench_find_if(c, iters, cname, is_negative<VT>(), "find_if(miss)");

   //find_if_not
   bench_find_if_not(c, iters, cname, unequal_to_ref<VT>(half), "find_if_not(hit)");
   bench_find_if_not(c, iters, cname, is_zero_or_positive<VT>(), "find_if_not(miss)");

#if !defined(BOOST_CONTAINER_BENCH_SEGMENTED_BIDIR_ENABLED) || BOOST_CONTAINER_BENCH_SEGMENTED_BIDIR_ENABLED
   //find_last
   bench_find_last(c, iters, cname, half, "find_last(hit)");
   bench_find_last(c, iters, cname, min1, "find_last(miss)");

   //find_last_if
   bench_find_last_if(c, iters, cname, equal_to_ref<VT>(half), "find_last_if(hit)");
   bench_find_last_if(c, iters, cname, is_negative<VT>(), "find_last_if(miss)");

   //find_last_if_not
   bench_find_last_if_not(c, iters, cname, unequal_to_ref<VT>(half), "find_last_if_not(hit)");
   bench_find_last_if_not(c, iters, cname, is_zero_or_positive<VT>(), "find_last_if_not(miss)");
#endif

   //for_each
   bench_for_each(c, iters, cname);

   //generate
   bench_generate(c, iters, cname);

   //generate_n
   bench_generate_n(c, iters, cname);

   //is_partitioned
   {
      bench_is_partitioned(c, iters, cname, is_negative<VT>(), "is_partitioned(hit)");
      C c2(c);
      *boost::container::make_iterator_uadvance(c2.begin(), c2.size()/2) = min1;
      bench_is_partitioned(c2, iters, cname, is_negative<VT>(), "is_partitioned(miss)");
   }

   //none_of
   bench_none_of(c, iters, cname, is_negative<VT>(), "none_of(hit)");
   bench_none_of(c, iters, cname, equal_to_ref<VT>(VT(static_cast<int>(c.size()/2))),      "none_of(miss)");

   #if !defined(BOOST_CONTAINER_BENCH_SEGMENTED_BIDIR_ENABLED) || BOOST_CONTAINER_BENCH_SEGMENTED_BIDIR_ENABLED
   //partition
   bench_partition(c, iters, cname, is_odd<VT>(),      "partition(hit)");
   bench_partition(c, iters, cname, is_negative<VT>(), "partition(miss)");
   #endif

   //replace
   {
      C c2(c);
      is_odd<VT> is_odd_pred;
      for (typename C::iterator it = c2.begin(); it != c2.end(); ++it){
         if( is_odd_pred(*it) )
            *it = min1;
      }
      bench_replace(c2, iters, cname, min1,  VT(-2),  "replace(hit)");
   }
   bench_replace(c, iters, cname, min1, VT(-2), "replace(miss)");

   //replace_if
   bench_replace_if(c, iters, cname, is_odd<VT>(),      VT(-2), "replace_if(hit)");
   bench_replace_if(c, iters, cname, is_negative<VT>(), VT(-2), "replace_if(miss)");

   //stable_partition (not tested since it's not optimized for random access iterators)
   //bench_stable_partition(c, iters, cname, is_odd<VT>(),      "stable_partition(hit)");
   //bench_stable_partition(c, iters, cname, is_negative<VT>(), "stable_partition(miss)");

#endif

   //////////////////////////////////////////////////////////////////
   // Group 1.5: Algorithms with 1 range but two iterational directions
   //////////////////////////////////////////////////////////////////
#if !defined(BOOST_CONTAINER_BENCH_SEGMENTED_GROUP) || BOOST_CONTAINER_BENCH_SEGMENTED_GROUP == 0 || BOOST_CONTAINER_BENCH_SEGMENTED_GROUP == 15

   //is_sorted
   {
      bench_is_sorted(c, iters, cname, "is_sorted(hit)");
      C c2(c);
      *boost::container::make_iterator_uadvance(c2.begin(), c2.size()/2) = min1;
      bench_is_sorted(c2, iters, cname, "is_sorted(miss)");
   }

   //is_sorted_until
   {
      bench_is_sorted_until(c, iters, cname, "is_sorted_until(hit)");
      C c2(c);
      *boost::container::make_iterator_uadvance(c2.begin(), c2.size()/2) = min1;
      bench_is_sorted_until(c2, iters, cname, "is_sorted_until(miss)");
   }

   //partition_point (not tested since it's not optimized for random access iterators)
   //bench_partition_point(c, iters, cname, less_than_ref<VT>(static_cast<VT>((int)c.size()/2)), "partition_point(hit)");
   //bench_partition_point(c, iters, cname, is_zero_or_positive<VT>(),                           "partition_point(miss)");

   //remove

   bench_remove(c, iters, cname, half,  "remove(hit)");
   bench_remove(c, iters, cname, min1,  "remove(miss)");

   //remove_if
   bench_remove_if(c, iters, cname, less_and_greater_ref<VT>(quart, threequart), "remove_if(hit)");
   bench_remove_if(c, iters, cname, is_negative<VT>(), "remove_if(miss)");

#if !defined(BOOST_CONTAINER_BENCH_SEGMENTED_BIDIR_ENABLED) || BOOST_CONTAINER_BENCH_SEGMENTED_BIDIR_ENABLED
   //reverse
   bench_reverse(c, iters, cname);
#endif

   //search_n
   bench_search_n(c, iters, cname, 1, half, "search_n(1hit)");
   bench_search_n(c, iters, cname, 3, min1, "search_n(miss)");

   //search_n with decoy runs + a real match of 8 in the middle
   {
      C c_sn(c);
      const std::size_t sz = c_sn.size();
      const std::size_t mid = sz / 2;

      // Place decoy runs of min1 in the first half (lengths 2, 4, 6)
      // spaced out so they don't overlap
      const std::size_t gap = mid / 4;
      const std::size_t decoy_runs[] = { 2, 4, 6 };
      for(std::size_t d = 0; d < 3; ++d) {
         const std::size_t pos = gap * (d + 1) - decoy_runs[d];
         typename C::iterator it = boost::container::make_iterator_uadvance(c_sn.begin(), pos);
         for(std::size_t k = 0; k < decoy_runs[d]; ++k, ++it)
            *it = min1;
      }

      // Place the real match of 8 consecutive min1 values at the middle
      {
         typename C::iterator it = boost::container::make_iterator_uadvance(c_sn.begin(), mid);
         for(std::size_t k = 0; k < 8; ++k, ++it)
            *it = min1;
      }

      bench_search_n(c_sn, iters, cname, 8, min1, "search_n(8mid)");
   }

#endif

   //////////////////////////////////////////////////////////////////
   // Group 2: Algorithms with 2 ranges
   //////////////////////////////////////////////////////////////////
#if !defined(BOOST_CONTAINER_BENCH_SEGMENTED_GROUP) || BOOST_CONTAINER_BENCH_SEGMENTED_GROUP == 0 || BOOST_CONTAINER_BENCH_SEGMENTED_GROUP == 20

   //copy
   bench_copy<C,     vec_t>(c,  iters, cname, "copy(1S)");
   bench_copy<vec_t, C    >(cv, iters, cname, "copy(2S)");
   bench_copy<C,     C    >(c,  iters, cname, "copy(2xS)");

   //copy_if
   bench_copy_if<C,     vec_t>(c,  iters, cname, is_odd<VT>(),      "copy_if(1S hit)");
   bench_copy_if<vec_t, C    >(cv, iters, cname, is_odd<VT>(),      "copy_if(2S hit)");
   bench_copy_if<C,     C    >(c,  iters, cname, is_odd<VT>(),      "copy_if(2xS hit)");
   bench_copy_if<C,     vec_t>(c,  iters, cname, is_negative<VT>(), "copy_if(1S miss)");
   bench_copy_if<vec_t, C    >(cv, iters, cname, is_negative<VT>(), "copy_if(2S miss)");
   bench_copy_if<C,     C    >(c,  iters, cname, is_negative<VT>(), "copy_if(2xS miss)");

   //copy_n
   bench_copy_n<C,     vec_t>(c,  iters, cname, "copy_n(1S)");
   bench_copy_n<vec_t, C    >(cv, iters, cname, "copy_n(2S)");
   bench_copy_n<C,     C    >(c,  iters, cname, "copy_n(2xS)");

   //equal
   {
      C c2(c);
      vec_t c2v(c2.begin(), c2.end());
      bench_equal<C,     vec_t>(c,  c2v, iters, cname, "equal(1S hit)");
      bench_equal<vec_t, C    >(cv, c2,  iters, cname, "equal(2S hit)");
      bench_equal<C,     C    >(c,  c2,  iters, cname, "equal(2xS hit)");
      *boost::container::make_iterator_uadvance(c2.begin(), c2.size()/2) = min1;
      c2v.assign(c2.begin(), c2.end());
      bench_equal<C,     vec_t>(c,  c2v, iters, cname, "equal(1S miss)");
      bench_equal<vec_t, C    >(cv, c2,  iters, cname, "equal(2S miss)");
      bench_equal<C,     C    >(c,  c2,  iters, cname, "equal(2xS miss)");
   }

   //mismatch
   {
      C c2(c);
      *boost::container::make_iterator_uadvance(c2.begin(), c2.size()/2) = min1;
      vec_t c2v(c2.begin(), c2.end());
      bench_mismatch<C,     vec_t>(c,  c2v, iters, cname, "mismatch(1S hit)");
      bench_mismatch<vec_t, C    >(cv, c2,  iters, cname, "mismatch(2S hit)");
      bench_mismatch<C,     C    >(c,  c2,  iters, cname, "mismatch(2xS hit)");
      *boost::container::make_iterator_uadvance(c2.begin(), c2.size()/2) =
         *boost::container::make_iterator_uadvance(c.begin(), c.size()/2);
      c2v.assign(c2.begin(), c2.end());
      bench_mismatch<C,     vec_t>(c,  c2v, iters, cname, "mismatch(1S miss)");
      bench_mismatch<vec_t, C    >(cv, c2,  iters, cname, "mismatch(2S miss)");
      bench_mismatch<C,     C    >(c,  c2,  iters, cname, "mismatch(2xS miss)");
   }

   //mismatch (two ranges)
   {
      C c2(c);
      *boost::container::make_iterator_uadvance(c2.begin(), c2.size()/2) = min1;
      vec_t c2v(c2.begin(), c2.end());
      bench_mismatch_2r<C,     vec_t>(c,  c2v, iters, cname, "mismatch_2r(1S hit)");
      bench_mismatch_2r<vec_t, C    >(cv, c2,  iters, cname, "mismatch_2r(2S hit)");
      bench_mismatch_2r<C,     C    >(c,  c2,  iters, cname, "mismatch_2r(2xS hit)");
      *boost::container::make_iterator_uadvance(c2.begin(), c2.size()/2) =
         *boost::container::make_iterator_uadvance(c.begin(), c.size()/2);
      c2v.assign(c2.begin(), c2.end());
      bench_mismatch_2r<C,     vec_t>(c,  c2v, iters, cname, "mismatch_2r(1S miss)");
      bench_mismatch_2r<vec_t, C    >(cv, c2,  iters, cname, "mismatch_2r(2S miss)");
      bench_mismatch_2r<C,     C    >(c,  c2,  iters, cname, "mismatch_2r(2xS miss)");
   }

   //remove_copy
   bench_remove_copy<C,     vec_t>(c,  iters, cname, half, "remove_copy(1S hit)");
   bench_remove_copy<vec_t, C    >(cv, iters, cname, half, "remove_copy(2S hit)");
   bench_remove_copy<C,     C    >(c,  iters, cname, half, "remove_copy(2xS hit)");
   bench_remove_copy<C,     vec_t>(c,  iters, cname, min1, "remove_copy(1S miss)");
   bench_remove_copy<vec_t, C    >(cv, iters, cname, min1, "remove_copy(2S miss)");
   bench_remove_copy<C,     C    >(c,  iters, cname, min1, "remove_copy(2xS miss)");

   //remove_copy_if
   bench_remove_copy_if<C,     vec_t>(c,  iters, cname, less_and_greater_ref<VT>(quart, threequart), "remove_copy_if(1S hit)");
   bench_remove_copy_if<vec_t, C    >(cv, iters, cname, less_and_greater_ref<VT>(quart, threequart), "remove_copy_if(2S hit)");
   bench_remove_copy_if<C,     C    >(c,  iters, cname, less_and_greater_ref<VT>(quart, threequart), "remove_copy_if(2xS hit)");
   bench_remove_copy_if<C,     vec_t>(c,  iters, cname, is_negative<VT>(), "remove_copy_if(1S miss)");
   bench_remove_copy_if<vec_t, C    >(cv, iters, cname, is_negative<VT>(), "remove_copy_if(2S miss)");
   bench_remove_copy_if<C,     C    >(c,  iters, cname, is_negative<VT>(), "remove_copy_if(2xS miss)");

#if !defined(BOOST_CONTAINER_BENCH_SEGMENTED_BIDIR_ENABLED) || BOOST_CONTAINER_BENCH_SEGMENTED_BIDIR_ENABLED
   //reverse_copy
   bench_reverse_copy<C,     vec_t>(c,  iters, cname, "reverse_copy(1S)");
   bench_reverse_copy<vec_t, C    >(cv, iters, cname, "reverse_copy(2S)");
   bench_reverse_copy<C,     C    >(c,  iters, cname, "reverse_copy(2xS)");
#endif

   //search
   {
      int ihalf = static_cast<int>(c.size() / 2);
      VT hit_pat[] = {half, VT(ihalf + 1), VT(ihalf + 2)};
      bench_search(c, iters, cname, hit_pat, 3, "search(hit)");
      VT miss_pat[] = {min1, VT(-2), VT(-3)};
      bench_search(c, iters, cname, miss_pat, 3, "search(miss)");
   }

   //swap_ranges
   bench_swap_ranges(c, iters, cname);

   //transform
   bench_transform(c, iters, cname);

#endif

   //////////////////////////////////////////////////////////////////
   // Group 3: Algorithms with 3 ranges
   //////////////////////////////////////////////////////////////////
#if !defined(BOOST_CONTAINER_BENCH_SEGMENTED_GROUP) || BOOST_CONTAINER_BENCH_SEGMENTED_GROUP == 0 || BOOST_CONTAINER_BENCH_SEGMENTED_GROUP == 30

   //merge
   {
      C c2(c);
      for (typename C::iterator it = c2.begin(); it != c2.end(); ++it)
         *it = VT(int_value(*it) * 2);
      vec_t c2v(c2.begin(), c2.end());

      bench_merge<C,     vec_t, vec_t>(c,  c2v, iters, cname, "merge(1S)");
      bench_merge<vec_t, C,     vec_t>(cv, c2,  iters, cname, "merge(2S)");
      bench_merge<vec_t, vec_t, C    >(cv, c2v, iters, cname, "merge(3S)");
      bench_merge<C,     C,     vec_t>(c,  c2,  iters, cname, "merge(2xS)");
      bench_merge<C,     C,     C    >(c,  c2,  iters, cname, "merge(3xS)");
   }

   //partition_copy
   bench_partition_copy(c, iters, cname);

   //set_difference, set_intersection, set_symmetric_difference, set_union
   {
      C c2(c);
      for (typename C::iterator it = c2.begin(); it != c2.end(); ++it)
         *it = VT(int_value(*it) * 2);
      vec_t c2v(c2.begin(), c2.end());

      // set_difference
      bench_set_difference<C,     vec_t, vec_t>(c,  c2v, iters, cname, "set_difference(1S)");
      bench_set_difference<vec_t, C,     vec_t>(cv, c2,  iters, cname, "set_difference(2S)");
      bench_set_difference<vec_t, vec_t, C    >(cv, c2v, iters, cname, "set_difference(3S)");
      bench_set_difference<C,     C,     vec_t>(c,  c2,  iters, cname, "set_difference(2xS)");
      bench_set_difference<C,     C,     C    >(c,  c2,  iters, cname, "set_difference(3xS)");

      // set_intersection
      bench_set_intersection<C,     vec_t, vec_t>(c,  c2v, iters, cname, "set_intersection(1S)");
      bench_set_intersection<vec_t, C,     vec_t>(cv, c2,  iters, cname, "set_intersection(2S)");
      bench_set_intersection<vec_t, vec_t, C    >(cv, c2v, iters, cname, "set_intersection(3S)");
      bench_set_intersection<C,     C,     vec_t>(c,  c2,  iters, cname, "set_intersection(2xS)");
      bench_set_intersection<C,     C,     C    >(c,  c2,  iters, cname, "set_intersection(3xS)");

      // set_symmetric_difference
      bench_set_symmetric_difference<C,     vec_t, vec_t>(c,  c2v, iters, cname, "set_sym_diff(1S)");
      bench_set_symmetric_difference<vec_t, C,     vec_t>(cv, c2,  iters, cname, "set_sym_diff(2S)");
      bench_set_symmetric_difference<vec_t, vec_t, C    >(cv, c2v, iters, cname, "set_sym_diff(3S)");
      bench_set_symmetric_difference<C,     C,     vec_t>(c,  c2,  iters, cname, "set_sym_diff(2xS)");
      bench_set_symmetric_difference<C,     C,     C    >(c,  c2,  iters, cname, "set_sym_diff(3xS)");

      // set_union
      bench_set_union<C,     vec_t, vec_t>(c,  c2v, iters, cname, "set_union(1S)");
      bench_set_union<vec_t, C,     vec_t>(cv, c2,  iters, cname, "set_union(2S)");
      bench_set_union<vec_t, vec_t, C    >(cv, c2v, iters, cname, "set_union(3S)");
      bench_set_union<C,     C,     vec_t>(c,  c2,  iters, cname, "set_union(2xS)");
      bench_set_union<C,     C,     C    >(c,  c2,  iters, cname, "set_union(3xS)");
   }

#endif

   std::cout << '\n';
   print_subheader();

   std::cout << std::left  << std::setw(24) << "algo geomean"
             << std::right << std::setw(16) << std::fixed << std::setprecision(2) << g_geomean.nsg_over_seg_result()
             << std::right << std::setw(16) << std::fixed << std::setprecision(2) << g_geomean.std_over_seg_result()
             << std::right << std::setw(16) << std::fixed << std::setprecision(2) << g_geomean.std_over_nsg_result()
             << '\n';
}

//////////////////////////////////////////////////////////////////////////////
// Run all benchmarks for a given value type
//////////////////////////////////////////////////////////////////////////////

template<class T>
void run_benchmarks()
{
#define BENCH_ON
   #if defined(NDEBUG) && defined(BENCH_ON)
   const std::size_t N    = 100000;
   const std::size_t iter = 3000;
   #else
   const std::size_t N    = 10000;
   const std::size_t iter = 1;
   #endif

   std::cout << "\n=== Segmented algorithm benchmark [" << typeid(T).name() << "] ===\n"
             << "Elements: " << N << "   Iterations: " << iter << "\n\n";

   {
      std::cout << "--- bc::deque<" << typeid(T).name() << "> ---\n";
      typedef typename bc::deque_options < bc::block_size<128> >::type block_size_opt_t;
      bc::deque<T, void, block_size_opt_t> dq;
      fill_test_data(dq, N);
      run_all(dq, iter, "deque");
         std::cout << "\n";
   }
/*
   {
      std::cout << "--- bc::nest<" << typeid(T).name() << "> ---\n";
      bc::nest<T> nt;
      fill_test_data(nt, N);
      run_all(nt, iter, "nest");
      std::cout << "\n";
   }
*/
}

//////////////////////////////////////////////////////////////////////////////
// Main
//////////////////////////////////////////////////////////////////////////////

int main()
{
   //run_benchmarks<int>();
   run_benchmarks<MyInt>();
   run_benchmarks<MyFatInt>();
   return 0;
}

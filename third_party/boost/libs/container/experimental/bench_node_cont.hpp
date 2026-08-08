/* Shared benchmark harness for node-like containers.
 *
 * Copyright 2026 Joaquin M Lopez Munoz.
 * Copyright 2026 Ion Gaztanaga.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * This header contains all the common machinery (element type, timing,
 * measurement, the individual tests and the reporting) used to compare two
 * containers ("num" and "den") and print the geometric means of the num/den
 * time ratios. A translation unit selects the two containers through a small
 * config type and calls bench_main<Config>():
 *
 *    struct config
 *    {
 *       template<class E> using num = boost::container::hub<E>;
 *       template<class E> using den = boost::container::nest<E>;
 *       static constexpr const char* num_name = "hub";
 *       static constexpr const char* den_name = "nest";
 *    };
 *    int main(){ return bench_main<config>(); }
 *
 */

#ifndef BOOST_CONTAINER_BENCH_NODE_CONT_HPP
#define BOOST_CONTAINER_BENCH_NODE_CONT_HPP

#include <boost/config.hpp>

#include <algorithm>
#include <numeric>
#include <iostream>
#include <boost/move/detail/nsec_clock.hpp>
#include "../bench/bench_utils.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

///////////////////////////////////////////////////////////////////////////////
// std::index_sequence / std::make_index_sequence are C++14; provide a minimal
// C++11-compatible equivalent so the harness builds with -std=c++11 too.
///////////////////////////////////////////////////////////////////////////////
namespace bench_detail {

template<std::size_t... Is>
struct index_sequence {};

template<std::size_t N, std::size_t... Is>
struct make_index_sequence_impl
   : make_index_sequence_impl<N - 1u, N - 1u, Is...> {};

template<std::size_t... Is>
struct make_index_sequence_impl<0u, Is...>
{  typedef index_sequence<Is...> type;  };

template<std::size_t N>
struct make_index_sequence
{  typedef typename make_index_sequence_impl<N>::type type;  };

}  //namespace bench_detail

///////////////////////////////////////////////////////////////////////////////
// Benchmark configuration knobs.
//
// A benchmark .cpp customizes the run by deriving its config from bench_defaults
// and redefining any of these members; every member has a default so a config
// only needs to override what it cares about:
//
//    struct config : bench_defaults
//    {
//       template<class E> using num = boost::container::hub<E>;
//       template<class E> using den = boost::container::nest<E>;
//       static constexpr const char* num_name = "hub";
//       static constexpr const char* den_name = "nest";
//       static constexpr std::size_t max_size_exp = 7;   //override one default
//    };
//
// bench_main<Config>() copies these into the runtime variables used throughout
// the harness, so the rest of the code keeps reading the plain globals below.
//
///////////////////////////////////////////////////////////////////////////////

struct bench_defaults
{
   //Element type: true benchmarks a non-trivial element (memset/memcpy work in
   //its special members); false a trivially copyable one. See element_t.
   static constexpr bool nontrivial_element = true;

   //Per measurement: number of timed trials (the slowest 25% are discarded)
   //and the minimum wall-clock time, in nanoseconds, each trial must run (the
   //measured operation is repeated in a loop until this time elapses).
#if defined(LONG_BENCH)
   static constexpr std::size_t num_trials = 8;
   static constexpr boost::move_detail::nanosecond_type
      min_time_per_trial = 100 * 1000000; //100 ms

   //Container sizes swept per table: 10^min_size_exp .. 10^max_size_exp.
   static constexpr std::size_t min_size_exp = 4;
   static constexpr std::size_t max_size_exp = 6;

   //Erase-rate sweep: from min to max (inclusive) in erase_rate_inc steps.
   static constexpr double min_erasure_rate = 0.1;
   static constexpr double max_erasure_rate = 0.9;
   static constexpr double erase_rate_inc   = 0.4;

#else
   //Container sizes swept per table: 10^min_size_exp .. 10^max_size_exp.
   static constexpr std::size_t min_size_exp = 4;
   static constexpr std::size_t max_size_exp = 4;

   //Erase-rate sweep: from min to max (inclusive) in erase_rate_inc steps.
   static constexpr double min_erasure_rate = 0.5;
   static constexpr double max_erasure_rate = 0.5;
   static constexpr double erase_rate_inc   = 0.1;

   static constexpr std::size_t num_trials = 1;
   static constexpr boost::move_detail::nanosecond_type
      min_time_per_trial = 0;
#endif
};

//Runtime mirrors of the bench_defaults knobs, set by bench_main<Config>() from
//the selected Config. They start at the defaults so the harness is usable even
//before bench_main runs.
static std::size_t min_size_exp = bench_defaults::min_size_exp;
static std::size_t max_size_exp = bench_defaults::max_size_exp;
static double      min_erasure_rate = bench_defaults::min_erasure_rate;
static double      max_erasure_rate = bench_defaults::max_erasure_rate;
static double      erase_rate_inc   = bench_defaults::erase_rate_inc;
static std::size_t bench_num_trials = bench_defaults::num_trials;
static boost::move_detail::nanosecond_type
                   bench_min_time_per_trial = bench_defaults::min_time_per_trial;

//Element sizes (in bytes) benchmarked. Each size produces its own table.
//Sizes must be >= sizeof(int); a size that is not a multiple of the int
//alignment is rounded up by the compiler, and the printed sizeof(element)
//reflects the actual (possibly padded) size. A .cpp may override the set by
//defining ELEMENT_SIZES before including this header.
#ifndef ELEMENT_SIZES
#ifdef LONG_BENCH
   //#define ELEMENT_SIZES { 32, 64, 96, 128, 192, 256 }
   #define ELEMENT_SIZES { 32, 64, 128, 256 }
   #define ELEMENT_SIZES { 64, 80 }
#  else //LONG_BENCH
   //#define ELEMENT_SIZES { 128 }
   #define ELEMENT_SIZES { 64 }
   //#define ELEMENT_SIZES { 32 }
#  endif //LONG_BENCH
#endif
static constexpr std::size_t element_sizes[] = ELEMENT_SIZES;
static constexpr std::size_t element_sizes_count =
   sizeof(element_sizes) / sizeof(element_sizes[0]);

//Wall-clock measured in nanoseconds via boost::move_detail::nsec_clock().
//measure_start is advanced forward by resume_timing() so that
//(now - measure_start) yields measured (non-paused) time.
static boost::move_detail::nanosecond_type measure_start, measure_pause;

template<typename F>
BOOST_NOINLINE double measure(F f)
{
   typedef boost::move_detail::nanosecond_type nsec_t;

   //Driven by the config (defaults from bench_defaults, which honor NDEBUG /
   //LONG_BENCH); guard against a zero trial count.
   const std::size_t num_trials = bench_num_trials ? bench_num_trials : 1;
   const nsec_t      min_time_per_trial = bench_min_time_per_trial;

   std::vector<double> trials(num_trials);

   for(std::size_t i = 0; i < num_trials; ++i) {
      int             runs = 0;
      nsec_t          t1;
      nsec_t          t2;
      decltype(f())   res;

      measure_start = t1 = boost::move_detail::nsec_clock();
      do{
         clobber();
         res = f();
         escape(&res);
         t2 = boost::move_detail::nsec_clock();
         ++runs;
      }while((t2 - t1) < min_time_per_trial);
      trials[i] = double(t2 - measure_start) / 1.0e9 / runs;
   }
   std::sort(trials.begin(), trials.end());

   const std::size_t ts = trials.size();
   const std::size_t ts_discard = ts/4;
   return std::accumulate(trials.begin() + std::ptrdiff_t(ts_discard), trials.end(), 0.0)/double(ts - ts_discard);
}

BOOST_CONTAINER_FORCEINLINE void pause_timing()
{
   measure_pause = boost::move_detail::nsec_clock();
}

BOOST_CONTAINER_FORCEINLINE void resume_timing()
{
   measure_start += boost::move_detail::nsec_clock() - measure_pause;
}

#include <boost/core/detail/splitmix64.hpp>
#include <cmath>
#include <cstring>
#include <iterator>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>

#if defined(PLF_HIVE_BENCH)
#include "plf_hive.h"
#endif

//Benchmark element (element_t<Size, NonTrivial>) is defined in bench_utils.hpp
//so it can be shared across benchmarks. The NonTrivial boolean is chosen here
//by Config::nontrivial_element (true by default).

struct urbg
{
   using result_type = boost::uint64_t;

   static constexpr result_type min() { return 0; }
   static constexpr result_type max() { return (result_type)(-1); }

   urbg() = default;
   explicit urbg(result_type seed): rng{seed} {}

   BOOST_CONTAINER_FORCEINLINE result_type operator()() { return rng(); }

   boost::detail::splitmix64 rng;
};

///////////////////////////////////////////////////////////////////////////////
// Container customization points.
//
// These let the harness work both with the segmented containers (hub/nest,
// which expose insert(value), erase_void, quick_emplace, a segmented for_each
// and shrink_to_fit) and with plain node containers such as boost::container::
// list / std::list (which only offer push_back, erase, sort and range
// iteration). Each helper prefers the richer member when available and falls
// back to the standard one otherwise (priority-tag dispatch: int beats long).
//
// Only the generic primary templates live here. A concrete benchmark can supply
// faster, container-specific overloads of erase_void / quick_emplace /
// quick_erase by declaring them in the namespace of the container it benchmarks
// (so they are found by ADL from the call sites below); see bench_hub.cpp for
// the hub/nest fast paths.
///////////////////////////////////////////////////////////////////////////////

//insert_one: insert a single element built from v and return its iterator.
template<typename Container, typename T>
BOOST_CONTAINER_FORCEINLINE
auto insert_one_impl(Container& x, const T& v, int) -> decltype(x.insert(v))
{
   return x.insert(v);
}

template<typename Container, typename T>
BOOST_CONTAINER_FORCEINLINE
typename Container::iterator insert_one_impl(Container& x, const T& v, long)
{
   x.push_back(v);
   return std::prev(x.end());
}

template<typename Container, typename T>
BOOST_CONTAINER_FORCEINLINE
typename Container::iterator insert_one(Container& x, const T& v)
{
   return insert_one_impl(x, v, 0);
}

//erase_void: erase without returning the next iterator (cheaper for hub/nest).
//Generic default; container-specific fast paths may be added via ADL by the
//concrete benchmark.
template<typename Container, typename Iterator>
BOOST_CONTAINER_FORCEINLINE void erase_void(Container& x, Iterator it)
{
   x.erase(it);
}

//quick_emplace: generic default reuses insert_one. A container with a faster
//rollback-free emplace (nest) provides its own overload in the concrete bench.
template<typename Container, typename T>
BOOST_CONTAINER_FORCEINLINE typename Container::iterator
quick_emplace(Container& x, const T& v)
{
   return insert_one(x, v);
}

//quick_erase: erase helper mirroring erase_void, used by the quick build path.
//Generic default; container-specific fast paths are added by the concrete bench.
template<typename Container, typename Iterator>
BOOST_CONTAINER_FORCEINLINE void quick_erase(Container& x, Iterator it)
{
   x.erase(it);
}

//bench_for_each: trampoline that keeps the harness container-agnostic. The
//default implementation is a plain range loop; if the container's namespace
//provides a free for_each(container, f) reachable via ADL (e.g. the segmented
//boost::container::for_each used by hub/nest), that faster version is used
//instead. The unqualified call below is what enables the ADL hook; std::for_each
//is not a candidate (different arity), so std::list falls back to the loop.
template<typename Container, typename F>
BOOST_CONTAINER_FORCEINLINE
auto bench_for_each_impl(Container& x, F& f, int)
   -> decltype(for_each(x, f), void())
{
   for_each(x, f);
}

template<typename Container, typename F>
BOOST_CONTAINER_FORCEINLINE
void bench_for_each_impl(Container& x, F& f, long)
{
   for(auto& e: x) f(e);
}

template<typename Container, typename F>
BOOST_CONTAINER_FORCEINLINE void bench_for_each(Container& x, F f)
{
   bench_for_each_impl(x, f, 0);
}

//bench_shrink_to_fit: no-op for containers (list, std::list) that lack it.
template<typename Container>
BOOST_CONTAINER_FORCEINLINE
auto bench_shrink_to_fit_impl(Container& x, int) -> decltype(x.shrink_to_fit(), void())
{
   x.shrink_to_fit();
}

template<typename Container>
BOOST_CONTAINER_FORCEINLINE void bench_shrink_to_fit_impl(Container&, long) {}

template<typename Container>
BOOST_CONTAINER_FORCEINLINE void bench_shrink_to_fit(Container& x)
{
   bench_shrink_to_fit_impl(x, 0);
}

///////////////////////////////////////////////////////////////////////////////
// Detection of the extended members.
//
// The "erasure" and "quick filling" tests exist to measure the container-
// specific fast paths (erase_void / quick_emplace). When neither benchmarked
// container provides the corresponding member, those tests would only repeat
// the plain erase / fill tests through the generic fallbacks, so run_bench
// skips them.
///////////////////////////////////////////////////////////////////////////////
template<typename C, typename = void>
struct has_erase_void: std::false_type {};

template<typename C>
struct has_erase_void<C, decltype((void)
   std::declval<C&>().erase_void(std::declval<typename C::iterator>()))>
   : std::true_type {};

template<typename C, typename = void>
struct has_quick_emplace: std::false_type {};

template<typename C>
struct has_quick_emplace<C, decltype((void)
   std::declval<C&>().quick_emplace(std::declval<const typename C::value_type&>()))>
   : std::true_type {};

//Probe callable used to detect a free for_each(container, f). It must be usable
//as the unary function for_each applies to each element.
struct for_each_probe { template<typename T> void operator()(T&) const {} };

//has_for_each detects, via the same ADL hook bench_for_each uses, whether the
//container's namespace provides a free for_each(container, f) (the segmented
//hub/nest algorithm). The unqualified call below resolves only through ADL at
//instantiation; std::for_each (3 arguments) is not a candidate, so std::list
//and boost::container::list report false.
template<typename C, typename = void>
struct has_for_each: std::false_type {};

template<typename C>
struct has_for_each<C, decltype((void)
   for_each(std::declval<C&>(), for_each_probe{}))>
   : std::true_type {};

///////////////////////////////////////////////////////////////////////////////
// Container builders.
///////////////////////////////////////////////////////////////////////////////

template<typename Container>
Container make(std::size_t n, double erasure_rate)
{
   std::uint64_t erasure_cut =
      (std::uint64_t)(erasure_rate * (double)(std::uint64_t)(-1));

   Container                                 c;
   urbg                                      rng;
   std::vector<typename Container::iterator> iterators;

   iterators.reserve(n);
   for(std::size_t i = 0; i < n; ++i) iterators.push_back(insert_one(c, (int)rng()));
   std::shuffle(iterators.begin(), iterators.end(), rng);
   for(auto it: iterators) {
      if(rng() < erasure_cut) erase_void(c, it);
   }
   return c;
}

template<typename Container>
void fill(Container& c, std::size_t n)
{
   urbg rng;
   if(n > c.size()) {
      n -= c.size();
      while(n--) insert_one(c, (int)rng());
   }
}

//Quick variants of make/fill that exercise the quick_emplace insertion path
//(nest::quick_emplace; insert for the other containers).
template<typename Container>
Container quick_make(std::size_t n, double erasure_rate)
{
   std::uint64_t erasure_cut =
      (std::uint64_t)(erasure_rate * (double)(std::uint64_t)(-1));

   Container                                 c;
   urbg                                      rng;
   std::vector<typename Container::iterator> iterators;

   iterators.reserve(n);
   for(std::size_t i = 0; i < n; ++i) iterators.push_back(quick_emplace(c, (int)rng()));
   std::shuffle(iterators.begin(), iterators.end(), rng);
   for(auto it: iterators) {
      if(rng() < erasure_cut) quick_erase(c, it);
   }
   return c;
}

template<typename Container>
void quick_fill(Container& c, std::size_t n)
{
   urbg rng;
   if(n > c.size()) {
      n -= c.size();
      while(n--) quick_emplace(c, (int)rng());
   }
}

struct benchmark_result
{
   std::string                           title;
   std::vector<std::vector<std::string>> data;
   std::vector<std::vector<double>>      ratios;
};

double geomean(const benchmark_result& bench);  // defined further below

template<typename FNum, typename FDen>
benchmark_result benchmark(const char* title, std::size_t element_size, FNum fnum, FDen fden)
{
   static constexpr std::size_t size_limit =
      sizeof(std::size_t) == 4?
#if defined(BOOST_MSVC) && defined(_M_IX86)
                                600ull * 1024ull * 1024ull:
#else
                                800ull * 1024ull * 1024ull:
#endif
                                2048ull * 1024ull * 1024ull;

   benchmark_result res = {title, {}, {}};

   //Wall-clock elapsed for this whole test (all erase rates x sizes), including
   //paused regions; this is the real time the test takes to run, not the
   //measured per-operation time used for the ratios.
   const boost::move_detail::nanosecond_type test_start = boost::move_detail::nsec_clock();

   char current_fill = std::cout.fill();
   std::cout << std::setfill('-') << std::setw(41) << "" <<"\n"
             << title << "\n"
             << "sizeof(element): " << element_size << "\n"
             << std::setfill(current_fill);
   std::cout << std::left << std::setw(11) << "" << "container size\n" << std::right
             << std::left << std::setw(11) << "erase rate" << std::right;
   for(std::size_t i = min_size_exp; i <= max_size_exp; ++i)
   {
      std::cout << "1.E" << i << " ";
   }
   std::cout << " mean" << std::endl;

   for(double erasure_rate = min_erasure_rate;
              erasure_rate <= max_erasure_rate;
              erasure_rate += erase_rate_inc) {
      //Print the erase rate in a self-contained format: the row-geomean
      //and final-geomean prints below leave std::cout in std::fixed with a
      //sticky precision, which would otherwise bleed into this column on
      //later rows/tables (e.g. "0.50"/"0.100"). Reset to defaultfloat and
      //round away the += accumulation noise so it always shows e.g. 0.1.
      std::cout << std::left << std::setw(11) << std::defaultfloat << std::setprecision(6)
                << (std::round(erasure_rate * 1000.0) / 1000.0)
                << std::right << std::flush;

      res.data.push_back({});
      res.ratios.push_back({});

      for(std::size_t i = min_size_exp; i <= max_size_exp; ++i) {
         std::ostringstream out;
         std::size_t        n = (std::size_t)std::pow(10.0, (double)i);
         if(n * element_size > size_limit) {
            out << "----";
         }
         else{
            auto tnum = measure([&] { return fnum(n, erasure_rate); });
            auto tden  = measure([&] { return fden(n, erasure_rate); });
            double ratio = tnum / tden;
            out << std::fixed << std::setprecision(2) << ratio;
            res.ratios.back().push_back(ratio);
         }
         std::cout << out.str() << " " << std::flush;
         res.data.back().push_back(out.str());
      }
      {  //Per-row geomean across the container sizes for this erase rate.
         double      row_log_sum = 0.0;
         std::size_t row_count   = 0;
         for(double r: res.ratios.back()) {
            if(r > 0.0) { row_log_sum += std::log(r); ++row_count; }
         }
         double row_geomean = row_count ? std::exp(row_log_sum / (double)row_count) : 0.0;
         std::cout << std::fixed << std::setprecision(2) << row_geomean;
      }
      std::cout << std::endl;
   }
   std::cout << std::left << std::setw(11) << "geomean"
             << std::right << std::fixed << std::setprecision(2)
             << geomean(res) << "\n";

   const double test_elapsed = (double)(boost::move_detail::nsec_clock() - test_start) / 1.0e9;
   std::cout << std::left << std::setw(11) << "elapsed"
             << std::right << std::fixed << std::setprecision(3)
             << test_elapsed << " s\n";
   return res;
}

template<typename Container>
struct create_fill
{
   unsigned int operator()(std::size_t n, double erasure_rate) const
   {
      unsigned int res = 0;
      {
         auto c = make<Container>(n, erasure_rate);
         fill(c, n);
         res = (unsigned int)c.size();
         pause_timing();
      }
      resume_timing();
      return res;
   }
};

template<typename Container>
struct create_fill_and_destroy
{
   unsigned int operator()(std::size_t n, double erasure_rate) const
   {
      auto c = make<Container>(n, erasure_rate);
      fill(c, n);
      return (unsigned int)c.size();
   }
};

//Isolates the cost of creating the container
template<typename Container>
struct creation
{
   unsigned int operator()(std::size_t n, double erasure_rate) const
   {
      unsigned int res = 0;
      {  //Construct 3 containers so that the destruction cost is more visible in the timing
         auto c1 = make<Container>(n, erasure_rate);   // measured
         auto c2 = make<Container>(n, erasure_rate);   // measured
         auto c3 = make<Container>(n, erasure_rate);   // measured
         res = (unsigned int)(c1.size()+c2.size()+c3.size());
         pause_timing();                              // exclude destruction
      }
      resume_timing();
      return res;
   }
};

//Isolates the cost of fill() alone (re-inserting up to n elements into an
//already-built, partially-erased container), excluding make and destruction.
template<typename Container>
struct filling
{
   unsigned int operator()(std::size_t n, double erasure_rate) const
   {
      unsigned int res = 0;
      {
         pause_timing();
         auto c1 = make<Container>(n, erasure_rate);   // excluded
         auto c2 = make<Container>(n, erasure_rate);   // excluded
         resume_timing();
         fill(c1, n);                                  // measured
         fill(c2, n);                                  // measured
         res = (unsigned int)(c1.size()+c2.size());
         pause_timing();                              // exclude destruction
      }
      resume_timing();
      return res;
   }
};

//Like filling, but the measured re-insertion uses the quick_emplace path
//(nest::quick_emplace; insert for hub/hive).
template<typename Container>
struct quick_filling
{
   unsigned int operator()(std::size_t n, double erasure_rate) const
   {
      unsigned int res = 0;
      {
         pause_timing();
         auto c1 = quick_make<Container>(n, erasure_rate);   // excluded
         auto c2 = quick_make<Container>(n, erasure_rate);   // excluded
         resume_timing();
         quick_fill(c1, n);                                  // measured
         quick_fill(c2, n);                                  // measured
         res = (unsigned int)(c1.size()+c2.size());
         pause_timing();                                    // exclude destruction
      }
      resume_timing();
      return res;
   }
};

template<typename Container>
struct erasure
{
   unsigned int operator()(std::size_t n, double erasure_rate) const
   {
      unsigned int res = 0;
      {
         pause_timing();
         std::uint64_t erasure_cut =
            (std::uint64_t)(erasure_rate * (double)(std::uint64_t)(-1));

         Container                                 c1;
         Container                                 c2;
         urbg                                      rng;
         std::vector<typename Container::iterator> iterators1;
         std::vector<typename Container::iterator> iterators2;

         iterators1.reserve(n);
         iterators2.reserve(n);
         for (std::size_t i = 0; i < n; ++i) {
            iterators1.push_back(insert_one(c1, (int)rng()));
            iterators2.push_back(insert_one(c2, (int)rng()));
         }

         std::shuffle(iterators1.begin(), iterators1.end(), rng);
         std::shuffle(iterators2.begin(), iterators2.end(), rng);
         resume_timing();

         for ( auto it1 = iterators1.begin()
             ; it1 != iterators1.end()
             ; ++it1) {
            if (rng() < erasure_cut) {
               erase_void(c1, *it1);
            }
         }

         for ( auto it2 = iterators2.begin()
             ; it2 != iterators2.end()
             ; ++it2) {
            if (rng() < erasure_cut) {
               erase_void(c2, *it2);
            }
         }

         pause_timing();
         res = (unsigned)c1.size() + (unsigned)c2.size();
      }
      return res;
   }
};

//Isolates the cost of the container destructor alone, over a fully built and
//filled container, excluding make and fill.
template<typename Container>
struct destruction
{
   unsigned int operator()(std::size_t n, double erasure_rate) const
   {
      unsigned int res = 0;
      {
         pause_timing();
         auto c = make<Container>(n, erasure_rate);   // excluded
         res = (unsigned int)c.size();
         resume_timing();                             // measure only the dtor below
      }                                               // ~Container() measured here
      return res;
   }
};

//Isolates the cost of clear() alone (destroys all elements but keeps the
//reserved capacity), excluding make and the final destruction.
template<typename Container>
struct clearing
{
   unsigned int operator()(std::size_t n, double erasure_rate) const
   {
      unsigned int res = 0;
      {
         pause_timing();
         auto c1 = make<Container>(n, erasure_rate);   // excluded
         auto c2 = make<Container>(n, erasure_rate);   // excluded
         auto c3 = make<Container>(n, erasure_rate);   // excluded
         resume_timing();
         c1.clear();                                   // measured
         c2.clear();                                   // measured
         c3.clear();                                   // measured
         res = (unsigned int)(c1.size() + c2.size() + c3.size());
         pause_timing();                              // exclude destruction
      }
      resume_timing();
      return res;
   }
};

template<typename Container>
struct prepare
{
   const Container& get_container(std::size_t n_arg, double erasure_rate_arg)
   {
      if(n_arg != n_ || erasure_rate_arg != erasure_rate_) {
         pause_timing();
         n_ = n_arg;
         erasure_rate_ = erasure_rate_arg;
         c.clear();
         bench_shrink_to_fit(c);
         c = make<Container>(n_, erasure_rate_);
         resume_timing();
      }
      return c;
   }

   std::size_t n_ = 0;
   double      erasure_rate_ = 0.0;
   Container   c;
};

template<typename Container>
struct iteration: prepare<Container>
{
   unsigned int operator()(std::size_t n, double erasure_rate)
   {
      unsigned int res = 0;
      auto& cr = this->get_container(n, erasure_rate);
      for(const auto& x: cr) res += (unsigned int)x;
      return res;
   }
};

template<typename Container>
struct for_each: prepare<Container>
{
   unsigned int operator()(std::size_t n, double erasure_rate)
   {
      unsigned int res = 0;
      auto& cr = this->get_container(n, erasure_rate);
      bench_for_each(const_cast<Container&>(cr), [&] (const typename Container::value_type& x) { res += (unsigned int)x; });
      return res;
   }
};

#if defined(PLF_HIVE_BENCH)

template<typename Element>
struct for_each <plf::hive<Element>>
   : iteration< plf::hive<Element> >
{};

#endif

template<typename Container>
struct sort
{
   unsigned int operator()(std::size_t n, double erasure_rate) const
   {
      pause_timing();
      auto c = make<Container>(n, erasure_rate);
      resume_timing();
      c.sort();
      return (unsigned int)c.size();
   }
};

using table = std::vector<benchmark_result>;

inline double geomean(const table& t)
{
   double log_sum = 0.0;
   std::size_t count = 0;
   for(const auto& bench: t) {
      for(const auto& row: bench.ratios) {
         for(double r: row) {
            if(r > 0.0) { log_sum += std::log(r); ++count; }
         }
      }
   }
   return count > 0 ? std::exp(log_sum / (double)count) : 0.0;
}

inline double geomean(const benchmark_result& bench)
{
   double log_sum = 0.0;
   std::size_t count = 0;
   for(const auto& row: bench.ratios) {
      for(double r: row) {
         if(r > 0.0) { log_sum += std::log(r); ++count; }
      }
   }
   return count > 0 ? std::exp(log_sum / (double)count) : 0.0;
}

//Geometric mean of a set of (positive) ratios.
inline double geomean_of(const std::vector<double>& v)
{
   double log_sum = 0.0;
   std::size_t count = 0;
   for(double r: v) {
      if(r > 0.0) { log_sum += std::log(r); ++count; }
   }
   return count > 0 ? std::exp(log_sum / (double)count) : 0.0;
}

//The list of erasure rates benchmarked, in the same order (and rounding) as the
//rows of benchmark_result::ratios.
inline std::vector<double> erasure_rates()
{
   std::vector<double> v;
   for(double erasure_rate = min_erasure_rate;
              erasure_rate <= max_erasure_rate;
              erasure_rate += erase_rate_inc) {
      v.push_back(std::round(erasure_rate * 1000.0) / 1000.0);
   }
   return v;
}

//Geomean of every erasure-rate row of a [erasure_rate][size] ratio matrix.
inline std::vector<double> per_rate_geomeans(const std::vector<std::vector<double> >& ratios)
{
   std::vector<double> out;
   for(const std::vector<double>& row: ratios) out.push_back(geomean_of(row));
   return out;
}

//Column width of the numeric ("gen" + per-rate) columns.
static const int geomean_col_w = 6;

//Header row: blank label cell, then "gen" and one column per erasure rate.
inline void print_geomean_header(const std::vector<double>& rates)
{
   std::cout << std::left << std::setw(30) << "" << std::setw(geomean_col_w) << "gen";
   for(double r: rates) {
      std::ostringstream o;
      o << std::defaultfloat << std::setprecision(6) << r;
      std::cout << std::left << std::setw(geomean_col_w) << o.str();
   }
   std::cout << "\n";
}

//Data row: label, general geomean and one geomean per erasure rate. Missing
//per-rate values (size_per_rate shorter than rates) are left blank.
inline void print_geomean_row(const std::string& label, double gen,
                              const std::vector<double>& per_rate, std::size_t rate_count)
{
   std::cout << std::left << std::setw(30) << label
             << std::fixed << std::setprecision(2)
             << std::setw(geomean_col_w) << gen;
   for(std::size_t ri = 0; ri < rate_count; ++ri) {
      if(ri < per_rate.size())
         std::cout << std::setw(geomean_col_w) << per_rate[ri];
      else
         std::cout << std::setw(geomean_col_w) << "";
   }
   std::cout << "\n";
}


//Per-execution (single element size) result summary: the geomean of each
//individual test plus the overall geomean across all tests.
struct run_summary
{
   std::vector<std::pair<std::string, double> > per_test;
   //Raw num/den ratios of each test, kept as [test][erasure_rate][size] so the
   //aggregated report can also break geomeans down per erasure rate.
   std::vector<std::vector<std::vector<double> > > per_test_ratios;
   double                                       overall;
   std::size_t                                  element_size;
};

//Runs the full benchmark suite for a single element size. The two containers
//compared (num/den) are taken from Config; element_t<Size> is the value type.
template<typename Config, std::size_t Size>
run_summary run_bench()
{
   using element = element_t<Size, Config::nontrivial_element>;
   using num = typename Config::template num<element>;
   using den = typename Config::template den<element>;

   const std::size_t element_size = sizeof(element);

   char current_fill = std::cout.fill();
   std::cout << "\n" << std::setfill('=') << std::setw(41) << "" << "\n"
             << "ELEMENT SIZE: " << element_size << " bytes\n"
             << std::setw(41)  << "" << "\n"
             << std::setfill(current_fill);

   table t;
   t.push_back(benchmark(
      "iteration", element_size,
      ::iteration<num>{}, ::iteration<den>{}));
   //for_each measures the segmented free for_each fast path; run it only when
   //num or den provides one, otherwise it would just duplicate the iteration
   //test through the generic range-loop fallback.
   BOOST_IF_CONSTEXPR(has_for_each<num>::value || has_for_each<den>::value) {
      t.push_back(benchmark(
         "for_each", element_size,
         ::for_each<num>{}, ::for_each<den>{}));
   }
   t.push_back(benchmark(
      "sort", element_size,
      sort<num>{}, sort<den>{}));
   t.push_back(benchmark(
      "create, fill", element_size,
      create_fill<num>{}, create_fill<den>{}));
   t.push_back(benchmark(
      "create, fill, destroy", element_size,
      create_fill_and_destroy<num>{}, create_fill_and_destroy<den>{}));
   t.push_back(benchmark(
      "destroy (dtor)", element_size,
      destruction<num>{}, destruction<den>{}));
   t.push_back(benchmark(
      "clear", element_size,
      clearing<num>{}, clearing<den>{}));
   t.push_back(benchmark(
      "creation (make)", element_size,
      creation<num>{}, creation<den>{}));
   t.push_back(benchmark(
      "filling", element_size,
      filling<num>{}, filling<den>{}));
   //The quick filling / erasure tests measure the quick_emplace / erase_void
   //fast paths; run them only when num or den actually provides the member,
   //otherwise they would just duplicate the filling / (plain) erase tests.
   BOOST_IF_CONSTEXPR(has_quick_emplace<num>::value || has_quick_emplace<den>::value) {
      t.push_back(benchmark(
         "quick filling", element_size,
         quick_filling<num>{}, quick_filling<den>{}));
   }
   BOOST_IF_CONSTEXPR(has_erase_void<num>::value || has_erase_void<den>::value) {
      t.push_back(benchmark(
         "erasure", element_size,
         ::erasure<num>{}, ::erasure<den>{}));
   }

   std::cout << "\n" << std::setfill('-') << std::setw(41) << "" "\n"
             << "Geometric means (num/den time ratio), element size "
             << element_size << "\n";
   std::cout << std::setfill(current_fill);

   const std::vector<double> rates = erasure_rates();
   print_geomean_header(rates);

   run_summary summary;
   for(const auto& bench: t) {
      const double g = geomean(bench);
      summary.per_test.push_back(std::make_pair(bench.title, g));
      summary.per_test_ratios.push_back(bench.ratios);
      print_geomean_row(bench.title, g, per_rate_geomeans(bench.ratios), rates.size());
   }
   summary.overall = geomean(t);
   summary.element_size = element_size;

   //OVERALL: general geomean and, per erasure rate, the geomean pooled across
   //every test at that rate.
   std::vector<std::vector<double> > overall_per_rate(rates.size());
   for(const auto& bench: t)
      for(std::size_t ri = 0; ri < bench.ratios.size() && ri < rates.size(); ++ri)
         for(double r: bench.ratios[ri]) overall_per_rate[ri].push_back(r);
   std::vector<double> overall_rate_gm;
   for(const std::vector<double>& v: overall_per_rate) overall_rate_gm.push_back(geomean_of(v));
   print_geomean_row("OVERALL", summary.overall, overall_rate_gm, rates.size());
   return summary;
}

template<typename Config, std::size_t... Is>
void run_all(bench_detail::index_sequence<Is...>)
{
   //Collect each execution's per-test and overall geomeans, then report
   //the geomean of each test across all executions, followed by the
   //geomean of the per-execution overall geomeans.
   const boost::move_detail::nanosecond_type total_start = boost::move_detail::nsec_clock();
   const run_summary summaries[] = { run_bench<Config, element_sizes[Is]>()... };
   const std::size_t num_exec = sizeof...(Is);

   char current_fill = std::cout.fill();
   std::cout << "\n\n\n"
             << std::setfill('=') << std::setw(41) << "" << "\n"
             << std::setfill('=') << std::setw(41) << "" << "\n"
             << "Aggregated geometric means across all " << num_exec
             << " executions (num/den time ratio)\n"
             << std::setfill('=') << std::setw(41) << "" << "\n"
             << std::setfill('=') << std::setw(41) << "" << "\n"
             << std::setfill(current_fill);

   const std::vector<double> rates = erasure_rates();

   //Per-test geomean across executions (test set is identical per execution),
   //with one column per erasure rate (pooled across executions and sizes).
   const std::size_t num_tests = summaries[0].per_test.size();
   std::cout << "\n ---- Per test ----\n";
   print_geomean_header(rates);
   for(std::size_t ti = 0; ti < num_tests; ++ti) {
      std::vector<double> vals;
      for(std::size_t e = 0; e < num_exec; ++e)
         vals.push_back(summaries[e].per_test[ti].second);

      std::vector<std::vector<double> > vals_per_rate(rates.size());
      for(std::size_t e = 0; e < num_exec; ++e) {
         const std::vector<std::vector<double> >& tr = summaries[e].per_test_ratios[ti];
         for(std::size_t ri = 0; ri < tr.size() && ri < rates.size(); ++ri)
            for(double r: tr[ri]) vals_per_rate[ri].push_back(r);
      }
      std::vector<double> rate_gm;
      for(const std::vector<double>& v: vals_per_rate) rate_gm.push_back(geomean_of(v));
      print_geomean_row(summaries[0].per_test[ti].first, geomean_of(vals),
                        rate_gm, rates.size());
   }

   //General (all-tests) geomean for each element size, shown only when more
   //than one element size was benchmarked, with one column per erasure rate.
   BOOST_IF_CONSTEXPR(num_exec > 1) {
      std::cout << "\n ---- Per size ----\n";
      print_geomean_header(rates);
      for(std::size_t e = 0; e < num_exec; ++e) {
         const std::string lbl = "element size " + std::to_string(summaries[e].element_size);

         std::vector<std::vector<double> > vals_per_rate(rates.size());
         for(std::size_t ti = 0; ti < num_tests; ++ti) {
            const std::vector<std::vector<double> >& tr = summaries[e].per_test_ratios[ti];
            for(std::size_t ri = 0; ri < tr.size() && ri < rates.size(); ++ri)
               for(double r: tr[ri]) vals_per_rate[ri].push_back(r);
         }
         std::vector<double> rate_gm;
         for(const std::vector<double>& v: vals_per_rate) rate_gm.push_back(geomean_of(v));
         print_geomean_row(lbl, summaries[e].overall, rate_gm, rates.size());
      }
   }

   //Geomean of the per-execution overall geomeans, with one column per erasure
   //rate (pooled across every execution, test and size at that rate).
   std::vector<double> overalls;
   for(std::size_t e = 0; e < num_exec; ++e)
      overalls.push_back(summaries[e].overall);

   std::vector<std::vector<double> > vals_per_rate(rates.size());
   for(std::size_t e = 0; e < num_exec; ++e) {
      for(std::size_t ti = 0; ti < num_tests; ++ti) {
         const std::vector<std::vector<double> >& tr = summaries[e].per_test_ratios[ti];
         for(std::size_t ri = 0; ri < tr.size() && ri < rates.size(); ++ri)
            for(double r: tr[ri]) vals_per_rate[ri].push_back(r);
      }
   }
   std::vector<double> rate_gm;
   for(const std::vector<double>& v: vals_per_rate) rate_gm.push_back(geomean_of(v));

   std::cout << '\n';
   print_geomean_header(rates);
   print_geomean_row("GEOMEAN OF GEOMEANS", geomean_of(overalls), rate_gm, rates.size());

   const double total_elapsed = (double)(boost::move_detail::nsec_clock() - total_start) / 1.0e9;
   std::cout << std::left << std::setw(30) << "TOTAL ELAPSED"
             << std::fixed << std::setprecision(3) << total_elapsed << " s\n";
}

//Entry point shared by every benchmark .cpp: runs the whole suite for the
//two containers described by Config across all configured element sizes.
template<typename Config>
int bench_main()
{
   //Apply the configuration knobs (defaults come from bench_defaults, which the
   //Config is expected to derive from; any of them may be overridden there).
   min_size_exp             = Config::min_size_exp;
   max_size_exp             = Config::max_size_exp;
   min_erasure_rate         = Config::min_erasure_rate;
   max_erasure_rate         = Config::max_erasure_rate;
   erase_rate_inc           = Config::erase_rate_inc;
   bench_num_trials         = Config::num_trials;
   bench_min_time_per_trial = Config::min_time_per_trial;

   std::cout << "Benchmark: " << Config::num_name << " (num) vs "
             << Config::den_name << " (den)\n";

   BOOST_CONTAINER_TRY{
      run_all<Config>(typename bench_detail::make_index_sequence<element_sizes_count>::type());
   }
   BOOST_CONTAINER_CATCH(const std::exception& e) {
      #ifndef BOOST_NO_EXCEPTIONS
      std::cerr << e.what() << std::endl;
      #endif
   }
   BOOST_CONTAINER_CATCH_END
   return 0;
}

#endif   //BOOST_CONTAINER_BENCH_NODE_CONT_HPP

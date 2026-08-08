//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_BENCH_UTILS_HPP
#define BOOST_CONTAINER_BENCH_UTILS_HPP

#include <boost/move/detail/nsec_clock.hpp>
#include <boost/move/utility_core.hpp>
#include <boost/container/detail/workaround.hpp>
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <vector>

volatile int bench_utils_sink = 0;

//#define BOOST_CONTAINER_BENCH_UTILS_NO_BARRIERS

#ifdef BOOST_CONTAINER_BENCH_UTILS_NO_BARRIERS
   #define BOOST_CONTAINER_BENCH_CLOBBER()   ((void)0)
   #define BOOST_CONTAINER_BENCH_ESCAPE(p)   ((void)(p))
#else
   #if defined(_MSC_VER)
      #define BOOST_CONTAINER_BENCH_CLOBBER()   _ReadWriteBarrier()
      #define BOOST_CONTAINER_BENCH_ESCAPE(p)   (bench_utils_sink = *static_cast<int*>(p))
   #elif defined(__GNUC__)
      #define BOOST_CONTAINER_BENCH_CLOBBER()   asm volatile("" : : : "memory")
      //#define BOOST_CONTAINER_BENCH_ESCAPE(p)   asm volatile("" : : "g"(p) : "memory")
      #define BOOST_CONTAINER_BENCH_ESCAPE(p)   asm volatile("" : "+r,m"(p) : : "memory")

   #else
      #define BOOST_CONTAINER_BENCH_CLOBBER()   ((void)0)
      #define BOOST_CONTAINER_BENCH_ESCAPE(p)   ((void)(p))
   #endif
#endif

#define BOOST_CONTAINER_BENCH_UTILS_USE_MEDIAN_CLOCK

#ifdef BOOST_CONTAINER_BENCH_UTILS_USE_MEDIAN_CLOCK

class cpu_timer
{
   typedef boost::move_detail::nanosecond_type nanosecond_type;

   nanosecond_type start_ns_;
   bool running_;
   mutable std::vector<nanosecond_type> samples_;

   BOOST_CONTAINER_FORCEINLINE static nanosecond_type now_ns()
   {
      return boost::move_detail::nsec_clock();
   }

   nanosecond_type robust_median() const
   {
      if(samples_.empty())
         return 0u;

      std::sort(samples_.begin(), samples_.end());

      std::size_t trim = samples_.size() / 20u; // Drop 5% low and high values.
      if(trim * 2u >= samples_.size())
         trim = 0u;

      const std::size_t begin = trim;
      const std::size_t end = samples_.size() - trim;
      const std::size_t count = end - begin;
      const std::size_t mid = begin + count / 2u;

      if((count % 2u) != 0u)
         return samples_[mid];

      return static_cast<nanosecond_type>((samples_[mid - 1u] + samples_[mid]) / 2u);
   }

   public:
   cpu_timer(std::size_t reserve = 0)
      : start_ns_(0u), running_(false), samples_()
   {  samples_.reserve(reserve); }

   BOOST_CONTAINER_FORCEINLINE bool is_stopped() const
   {  return !running_;  }

   nanosecond_type elapsed() const
   {
      return robust_median() * static_cast<nanosecond_type>(samples_.size());
   }

   void start()
   {
      samples_.clear();
      start_ns_ = now_ns();
      running_ = true;
   }
 
   void stop()
   {
      if(!running_)
         return;
      const nanosecond_type now = now_ns();
      samples_.push_back(now - start_ns_);
      running_ = false;
   }

   void resume()
   {
      if(running_)
         return;
      start_ns_ = now_ns();
      running_ = true;
   }
};

#else

typedef boost::move_detail::cpu_timer cpu_timer;

#endif


BOOST_CONTAINER_FORCEINLINE void clobber()       { BOOST_CONTAINER_BENCH_CLOBBER(); }
BOOST_CONTAINER_FORCEINLINE void escape(void* p) { BOOST_CONTAINER_BENCH_ESCAPE(p); }

///////////////////////////////////////////////////////////////////////////////
// Shared benchmark element types.
//
// MyInt: a non-trivial "int wrapper" used by several benchmarks to exercise the
// non-trivially-copyable element code paths (its user-provided copy ctor, copy
// assignment and destructor disable the trivial memcpy/relocation fast paths),
// as opposed to a plain int. This single definition replaces the per-benchmark
// copies that used to define their own MyInt. A benchmark that additionally
// wants the "trivial destructor after move" optimization can still specialize
// boost::has_trivial_destructor_after_move<MyInt> in its own translation unit.
///////////////////////////////////////////////////////////////////////////////
class MyInt
{
   int int_;

   public:
   MyInt(int i = 0)
      : int_(i)
   {}

   MyInt(const MyInt &other)
      : int_(other.int_)
   {}

   MyInt & operator=(const MyInt &other)
   {
      int_ = other.int_;
      return *this;
   }

   ~MyInt()
   {
      int_ = 0;
   }

   int int_value() const { return int_; }

   friend bool operator==(const MyInt& a, const MyInt& b) { return a.int_ == b.int_; }
   friend bool operator!=(const MyInt& a, const MyInt& b) { return a.int_ != b.int_; }
   friend bool operator< (const MyInt& a, const MyInt& b) { return a.int_ <  b.int_; }
   friend bool operator> (const MyInt& a, const MyInt& b) { return a.int_ >  b.int_; }
   friend bool operator<=(const MyInt& a, const MyInt& b) { return a.int_ <= b.int_; }
   friend bool operator>=(const MyInt& a, const MyInt& b) { return a.int_ >= b.int_; }
};

//Benchmark element. The NonTrivial boolean (chosen by the caller, true by
//default) selects between two layouts of identical size:
// - NonTrivial == true: a move-only element whose user-defined special members
//   do measurable work (memset/memcpy of the payload, kept alive by a compiler
//   barrier). It uses Boost.Move so the definition is also valid in C++03.
// - NonTrivial == false: a trivially copyable element carrying only the payload.
template<std::size_t Size, bool NonTrivial = true>
struct element_t;

template<std::size_t Size>
struct element_t<Size, true>
{
   BOOST_MOVABLE_BUT_NOT_COPYABLE(element_t)

   public:
   element_t(int n_) : n(n_)
   {
      std::memset(payload, 0, sizeof(payload));
      clobber();  //The barrier keeps previous writes as real work.
   }

   ~element_t()
   {
      std::memset(payload, 0, sizeof(payload));
      clobber();  //The barrier keeps previous writes as real work.
   }

   element_t(BOOST_RV_REF(element_t) x) : n(x.n)
   {
      std::memcpy(payload, x.payload, sizeof(payload));
      std::memset(x.payload, 0, sizeof(payload));
      clobber();  //The barrier keeps previous writes as real work.
   }

   element_t& operator=(BOOST_RV_REF(element_t) x)
   {
      n = x.n;
      std::memcpy(payload, x.payload, sizeof(payload));
      std::memset(x.payload, 0, sizeof(payload));
      clobber();  //The barrier keeps previous writes as real work.
      return *this;
   }

   operator int() const { return n; }

   int n;
   char payload[Size - sizeof(int)];
};

template<std::size_t Size>
struct element_t<Size, false>
{
   element_t(int n_) : n(n_)
   {}

   operator int() const { return n; }

   int n;
   char payload[Size - sizeof(int)];
};

#endif   //BOOST_CONTAINER_BENCH_UTILS_HPP

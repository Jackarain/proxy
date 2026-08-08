//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2007-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
//
// Shared harness for the sequence-container insertion benchmarks.
//
// Design:
//  - Auto-scaling measurement: each timing repeats the build until a minimum
//    wall-clock budget elapses, runs several trials and discards the slowest
//  - Dead-store-elimination barriers: clobber()/escape() (bench_utils.hpp) wrap
//    the measured region so the optimizer cannot delete the work.
//  - Ratio output: every container is timed and reported as a num/den ratio
//    against the first container the runner registers (the baseline), with a
//    per-column geometric mean. A compact table is printed per element type and
//    prereserve setting.
//
// A runner includes this header, implements the run_containers() customization
// point registering the containers it wants to compare (the FIRST one is the
// baseline/denominator) and calls test_vectors<int>():
//
//    #include "bench_vector_common.hpp"
//    template<class IntType, class Operation>
//    void run_containers(runner<IntType, Operation>& r)
//    {
//       r.template add< std::vector<IntType> >("std::vector");  //baseline
//       r.template add< bc::vector<IntType>  >("vector");
//    }
//    int main(){ test_vectors<int>(); return 0; }
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_BENCH_VECTOR_COMMON_HPP
#define BOOST_CONTAINER_BENCH_VECTOR_COMMON_HPP

#include <boost/config.hpp>
#include <boost/container/detail/workaround.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <typeinfo>
#include <vector>

#include <boost/move/detail/nsec_clock.hpp>
#include <boost/container/vector.hpp>
#include <boost/container/string.hpp>
#include "bench_utils.hpp"   //clobber(), escape()

#if defined(BOOST_GCC) && (BOOST_GCC >= 40600)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
#endif

//capacity
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME capacity
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_BEG namespace boost { namespace container { namespace test {
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_END   }}}
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MIN 0
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MAX 0
#include <boost/intrusive/detail/has_member_function_callable_with.hpp>

//reserve
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME reserve
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_BEG namespace boost { namespace container { namespace test {
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_END   }}}
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MIN 1
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MAX 1
#include <boost/intrusive/detail/has_member_function_callable_with.hpp>

//back_reserve
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME reserve_back
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_BEG namespace boost { namespace container { namespace test {
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_END   }}}
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MIN 1
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MAX 1
#include <boost/intrusive/detail/has_member_function_callable_with.hpp>

#if defined(BOOST_GCC) && (BOOST_GCC >= 40600)
#pragma GCC diagnostic pop
#endif

namespace bc = boost::container;

///////////////////////////////////////////////////////////////////////////////
// Benchmark configuration knobs.
//
// Container sizes are swept as 10^min_size_exp .. 10^max_size_exp. The defaults
// follow the same NDEBUG / LONG_BENCH profile idea used by bench_node_cont.hpp.
//
// NOTE: fixed-capacity containers (e.g. static_vector) are sized with
// bench_max_numele, which is derived from max_size_exp, so the two stay in sync.
///////////////////////////////////////////////////////////////////////////////
struct bench_vector_defaults
{
   typedef boost::move_detail::nanosecond_type nanosecond_type;

#if defined(LONG_BENCH)
   static const std::size_t min_size_exp = 2;
   static const std::size_t max_size_exp = 5;
   static const std::size_t num_trials   = 6;
   //50 ms per trial
   static const nanosecond_type min_time_per_trial = nanosecond_type(50) * 1000000;
#else
   static const std::size_t min_size_exp = 3;
   static const std::size_t max_size_exp = 3;
   static const std::size_t num_trials   = 1;
   static const nanosecond_type min_time_per_trial = 0;
#endif

   //Which prereserve passes to run (replaces the old RESERVE_STRATEGY macros).
   static const bool run_noreserve  = true;
   static const bool run_prereserve = true;

   //Range length used by the *_range / *_repeated operations, expressed as a
   //divisor of the final container size: range = max(1, n / range_divisor).
   //A divisor of 100 means each range insertion is 1% of the final size.
   static const std::size_t range_divisor = 100;
};

//Range length for a given final size n, following the bench_vector_defaults
//policy (fixed range_size, or proportional via range_divisor).
inline std::size_t bench_range_size(std::size_t n)
{
   //Guard against a zero divisor (avoids UB and the compile-time div-by-zero
   //diagnostic should the config ever be set to 0).
   const std::size_t div =
      bench_vector_defaults::range_divisor ? bench_vector_defaults::range_divisor : std::size_t(1);
   const std::size_t r = n / div;
   return r ? r : std::size_t(1);
}

//Human-readable description of the range policy, e.g. "n/100".
inline bc::string bench_range_label()
{
   std::ostringstream o;
   o << "n/" << bench_vector_defaults::range_divisor;
   return bc::string(o.str().c_str());
}

//Runtime 10^e (e is small here), used where e is only known at run time.
inline BOOST_CONSTEXPR std::size_t bench_pow10(std::size_t e)
{  return e ? std::size_t(10) * bench_pow10(e - 1u) : std::size_t(1);  }

//Compile-time 10^E as an integral constant expression. Unlike bench_pow10
//above this works in C++03 too (where a function call is not a constant
//expression), so bench_max_numele is usable as static_vector's capacity.
template<std::size_t E>
struct bench_pow10_c
{  static const std::size_t value = std::size_t(10) * bench_pow10_c<E - 1u>::value;  };

template<>
struct bench_pow10_c<0u>
{  static const std::size_t value = std::size_t(1);  };

//Largest element count exercised by the harness. Fixed-capacity containers
//(e.g. static_vector) must be sized at least this big.
BOOST_CONSTEXPR_OR_CONST std::size_t bench_max_numele =
   bench_pow10_c<bench_vector_defaults::max_size_exp>::value;

///////////////////////////////////////////////////////////////////////////////
// reserve()/capacity() abstraction (some containers offer reserve_back instead,
// some offer neither).
///////////////////////////////////////////////////////////////////////////////
template<class C, bool Capacity, bool BackCapacity>
struct capacity_wrapper_impl
{
   inline static typename C::size_type get_capacity(const C &)
   {  return 0u; }

   inline static void set_reserve(C &, typename C::size_type )
   { }
};

template<class C>
struct capacity_wrapper_impl<C, true, false>
{
   inline static typename C::size_type get_capacity(const C &c)
   {  return c.capacity(); }

   inline static void set_reserve(C &c, typename C::size_type cp)
   {  c.reserve(cp); }
};

template<class C, bool Capacity>
struct capacity_wrapper_impl<C, Capacity, true>
{
   inline static typename C::size_type get_capacity(const C &c)
   {  return c.back_capacity(); }

   inline static void set_reserve(C &c, typename C::size_type cp)
   {  c.reserve_back(cp); }
};

template<class C>
struct capacity_wrapper
   : capacity_wrapper_impl
      < C
      , bc::test::has_member_function_callable_with_reserve<C, std::size_t>::value
      , bc::test::has_member_function_callable_with_reserve_back<C, std::size_t>::value
      >
{};

///////////////////////////////////////////////////////////////////////////////
// Operations. Each functor exposes a uniform interface:
//   explicit Op(std::size_t n):  n is the final container size; range-based ops
//                                use it to size their range (see bench_range_size).
//   capacity_multiplier():       elements inserted per operator() call.
//   operator()(C&, int):         one insertion step.
//   name():                      short label for the report.
//
// The range-based operations keep a runtime buffer (their range length is no
// longer a compile-time constant), so they are constructed once per measured
// build, outside the timed region.
///////////////////////////////////////////////////////////////////////////////
template <class IntType>
struct insert_end_range
{
   explicit insert_end_range(std::size_t n)
      : range_(bench_range_size(n)), a_(range_, IntType(0))
   {}

   std::size_t capacity_multiplier() const
   {  return range_;  }

   template<class C>
   BOOST_CONTAINER_FORCEINLINE void operator()(C &c, int)
   {  c.insert(c.end(), &a_[0], &a_[0]+range_); }

   bc::string name() const
   {  return "insert_end_range(" + bench_range_label() + ")"; }

   std::size_t          range_;
   bc::vector<IntType>  a_;
};

template <class IntType>
struct insert_end_repeated
{
   explicit insert_end_repeated(std::size_t n)
      : range_(bench_range_size(n))
   {}

   std::size_t capacity_multiplier() const
   {  return range_;  }

   template<class C>
   BOOST_CONTAINER_FORCEINLINE void operator()(C &c, int i)
   {  c.insert(c.end(), range_, IntType(i)); }

   bc::string name() const
   {  return "insert_end_repeated(" + bench_range_label() + ")"; }

   std::size_t range_;
};

template <class IntType>
struct push_back
{
   explicit push_back(std::size_t) {}

   std::size_t capacity_multiplier() const
   {  return 1;  }

   template<class C>
   BOOST_CONTAINER_FORCEINLINE void operator()(C &c, int i)
   {  c.push_back(IntType(i)); }

   bc::string name() const
   {  return "push_back"; }
};

template <class IntType>
struct emplace_back
{
   explicit emplace_back(std::size_t) {}

   std::size_t capacity_multiplier() const
   {  return 1;  }

   template<class C>
   BOOST_CONTAINER_FORCEINLINE void operator()(C &c, int i)
   {  c.emplace_back(IntType(i)); }

   bc::string name() const
   {  return "emplace_back"; }
};

template <class IntType>
struct insert_near_end_rpt
{
   explicit insert_near_end_rpt(std::size_t n)
      : range_(bench_range_size(n))
   {}

   std::size_t capacity_multiplier() const
   {  return range_;  }

   template<class C>
   BOOST_CONTAINER_FORCEINLINE void operator()(C &c, int i)
   {
      c.insert(c.size() >= 4*range_
                  ? c.end() - static_cast<typename C::difference_type>(2*range_)
                  : c.end(),
               range_, IntType(i));
   }

   bc::string name() const
   {  return "insert_near_end_rpt(" + bench_range_label() + ")"; }

   std::size_t range_;
};

template <class IntType>
struct insert_near_end_range
{
   explicit insert_near_end_range(std::size_t n)
      : range_(bench_range_size(n)), a_(range_, IntType(0))
   {}

   std::size_t capacity_multiplier() const
   {  return range_;  }

   template<class C>
   BOOST_CONTAINER_FORCEINLINE void operator()(C &c, int)
   {
      c.insert(c.size() >= 4*range_
                  ? c.end() - static_cast<typename C::difference_type>(2*range_)
                  : c.end(),
               &a_[0], &a_[0]+range_);
   }

   bc::string name() const
   {  return "insert_near_end_range(" + bench_range_label() + ")"; }

   std::size_t          range_;
   bc::vector<IntType>  a_;
};

template <class IntType>
struct insert_near_end
{
   explicit insert_near_end(std::size_t) {}

   std::size_t capacity_multiplier() const
   {  return 1;  }

   template<class C>
   BOOST_CONTAINER_FORCEINLINE void operator()(C &c, int i)
   {
      typedef typename C::iterator it_t;
      it_t it (c.end());
      it -= static_cast<typename C::difference_type>(c.size() >= 4)*2;
      c.insert(it, IntType(i));
   }

   bc::string name() const
   {  return "insert_near_end"; }
};

template <class IntType>
struct emplace_near_end
{
   explicit emplace_near_end(std::size_t) {}

   std::size_t capacity_multiplier() const
   {  return 1;  }

   template<class C>
   BOOST_CONTAINER_FORCEINLINE void operator()(C& c, int i)
   {
      typedef typename C::iterator it_t;
      it_t it(c.end());
      it -= static_cast<typename C::difference_type>(c.size() >= 4) * 2;
      c.emplace(it, IntType(i));
   }

   bc::string name() const
   {  return "emplace_near_end"; }
};

///////////////////////////////////////////////////////////////////////////////
// Auto-scaling measurement with DSE barriers.
//
// A "measured run" builds one container of num_elements via repeated operations.
// Construction, the optional reserve and the destruction are excluded from the
// timing using the pause/resume mechanism (the same approach as
// bench_node_cont.hpp): the measured wall-clock is total - excluded.
///////////////////////////////////////////////////////////////////////////////
namespace bench_vector_detail {

typedef boost::move_detail::nanosecond_type nsec_t;

//measure_start is advanced forward by resume_timing() so that
//(now - measure_start) yields measured (non-paused) time.
static nsec_t measure_start = 0;
static nsec_t measure_pause = 0;

BOOST_CONTAINER_FORCEINLINE void pause_timing()
{  measure_pause = boost::move_detail::nsec_clock(); }

BOOST_CONTAINER_FORCEINLINE void resume_timing()
{  measure_start += boost::move_detail::nsec_clock() - measure_pause; }

template<typename F>
BOOST_NOINLINE double measure(F f, std::size_t num_trials, nsec_t min_time_per_trial)
{
   if(!num_trials) num_trials = 1;

   bc::vector<double> trials(num_trials);
   for(std::size_t i = 0; i < num_trials; ++i) {
      std::size_t runs = 0;
      nsec_t      t1;
      nsec_t      t2;

      measure_start = t1 = boost::move_detail::nsec_clock();
      do {
         clobber();
         unsigned res = f();
         escape(&res);
         t2 = boost::move_detail::nsec_clock();
         ++runs;
      } while((t2 - t1) < min_time_per_trial);
      trials[i] = double(t2 - measure_start) / 1.0e9 / double(runs);
   }
   std::sort(trials.begin(), trials.end());

   const std::size_t ts         = trials.size();
   const std::size_t ts_discard = ts / 3;   //drop the slowest
   double sum = 0.0;
   for(std::size_t i = ts_discard; i < ts; ++i) sum += trials[i];
   return sum / double(ts - ts_discard);
}

//Builds one Container of n elements with Operation, timing only the inserts.
//The Operation (which may own a runtime range buffer) and the reserve are set
//up in the paused region so only the insertions are measured.
template<class Container, class Operation>
unsigned build_once(std::size_t n, bool prereserve)
{
   typedef capacity_wrapper<Container> cpw_t;
   unsigned res = 0;
   {
      Container c;
      pause_timing();                                 //exclude ctor + reserve + op setup
      Operation         op(n);
      const std::size_t mult  = op.capacity_multiplier();
      const std::size_t count = mult ? n / mult : 0u;
      if(prereserve) cpw_t::set_reserve(c, n);
      resume_timing();

      int i = 0;
      for(std::size_t e = 0; e < count; ++e)
         op(c, static_cast<int>(i++));

      res = static_cast<unsigned>(c.size());
      pause_timing();                                 //exclude destruction
   }                                                  //~Container()/~Operation() (paused)
   resume_timing();
   return res;
}

}  //namespace bench_vector_detail

///////////////////////////////////////////////////////////////////////////////
// Friendly type names for the report header.
///////////////////////////////////////////////////////////////////////////////
template<class T> inline const char* bench_type_name() { return typeid(T).name(); }
template<> inline const char* bench_type_name<int>()          { return "int"; }
template<> inline const char* bench_type_name<unsigned>()     { return "unsigned"; }
template<> inline const char* bench_type_name<long>()         { return "long"; }
template<> inline const char* bench_type_name<float>()        { return "float"; }
template<> inline const char* bench_type_name<double>()       { return "double"; }
#ifdef BOOST_HAS_LONG_LONG
template<> inline const char* bench_type_name< ::boost::long_long_type>()  { return "long long"; }
#endif

///////////////////////////////////////////////////////////////////////////////
// Report: collects per (operation,size) seconds for every container column and
// prints a compact ratio table (each column divided by the first/baseline).
///////////////////////////////////////////////////////////////////////////////
inline bc::string bench_fmt2(double v)
{
   std::ostringstream o;
   o << std::fixed << std::setprecision(2) << v;
   return bc::string(o.str().c_str());
}

class report
{
   public:
   struct row
   {
      bc::string         op;
      std::size_t        size_exp;
      std::size_t        n_eff;
      bc::vector<double> sec;   //one per column; sec[0] is the baseline
   };

   report(const char* elem_name, bool prereserve)
      : elem_name_(elem_name), prereserve_(prereserve)
   {}

   bool has_columns() const { return !cols_.empty(); }

   void set_columns(const bc::vector<bc::string>& names) { cols_ = names; }

   void add_row(const bc::string& op, std::size_t size_exp,
                std::size_t n_eff, const bc::vector<double>& sec)
   {
      row r;
      r.op = op;
      r.size_exp = size_exp;
      r.n_eff = n_eff;
      r.sec = sec;
      rows_.push_back(r);
   }

   void print() const
   {
      const int op_w = 30, size_w = 7, col_w = 14;
      //The baseline column (index 0) is always 1.0, so it is not printed.
      const int printed_cols = cols_.empty() ? 0 : static_cast<int>(cols_.size()) - 1;
      const int line_w = op_w + size_w + col_w * printed_cols;

      std::cout << "\n" << bc::string(41, '=') << "\n"
                << "element=" << elem_name_
                << "  prereserve=" << (prereserve_ ? "1" : "0") << "\n";
      if(!cols_.empty())
         std::cout << "ratio vs '" << cols_[0] << "' (denominator), lower is faster\n";
      std::cout << bc::string(41, '=') << "\n";

      //Header: operation, size and one column per container (baseline omitted).
      std::cout << std::left << std::setw(op_w) << "operation"
                << std::right << std::setw(size_w) << "size";
      for(std::size_t c = 1; c < cols_.size(); ++c)
         std::cout << std::setw(col_w) << cols_[c];
      std::cout << "\n";

      //Data rows, grouped per operation. After each operation's size sweep a
      //separator and a "geomeans" line (per-column geomean across that
      //operation's sizes) are printed, followed by a blank separating line.
      std::size_t i = 0;
      while(i < rows_.size()) {
         std::size_t j = i;
         while(j < rows_.size() && rows_[j].op == rows_[i].op) ++j;

         for(std::size_t k = i; k < j; ++k) {
            const row& r = rows_[k];
            std::ostringstream se;
            se << "1.E" << r.size_exp;
            std::cout << std::left << std::setw(op_w) << r.op
                      << std::right << std::setw(size_w) << se.str();
            const double base = r.sec.empty() ? 0.0 : r.sec[0];
            for(std::size_t c = 1; c < r.sec.size(); ++c) {
               const double ratio = base > 0.0 ? r.sec[c] / base : 0.0;
               std::cout << std::setw(col_w) << bench_fmt2(ratio);
            }
            std::cout << "\n";
         }

         std::cout << bc::string(static_cast<bc::string::size_type>(line_w), '-') << "\n";
         std::cout << std::left << std::setw(op_w) << "geomeans"
                   << std::right << std::setw(size_w) << "";
         for(std::size_t c = 1; c < cols_.size(); ++c)
            std::cout << std::setw(col_w) << bench_fmt2(column_geomean_range(c, i, j));
         std::cout << "\n\n";

         i = j;
      }

      //Overall footer: per-column geomean across every operation/size, then the
      //general geomean over every ratio cell of the table on its own line.
      std::cout << bc::string(static_cast<bc::string::size_type>(line_w), '-') << "\n";
      std::cout << std::left << std::setw(op_w) << "geomean (all)"
                << std::right << std::setw(size_w) << "";
      for(std::size_t c = 1; c < cols_.size(); ++c)
         std::cout << std::setw(col_w) << bench_fmt2(column_geomean(c));
      std::cout << "\n";
      std::cout << std::left << std::setw(op_w) << "general geomean"
                << std::right << bench_fmt2(general_geomean()) << "\n";
   }

   private:
   //Geomean of one container column's ratios over the row range [begin, end).
   double column_geomean_range(std::size_t c, std::size_t begin, std::size_t end) const
   {
      double      log_sum = 0.0;
      std::size_t count   = 0;
      for(std::size_t i = begin; i < end; ++i) {
         const row& r = rows_[i];
         if(c < r.sec.size() && !r.sec.empty() && r.sec[0] > 0.0 && r.sec[c] > 0.0) {
            log_sum += std::log(r.sec[c] / r.sec[0]);
            ++count;
         }
      }
      return count ? std::exp(log_sum / double(count)) : 0.0;
   }

   //Geomean of one container column's ratios across all rows (vertical).
   double column_geomean(std::size_t c) const
   {  return column_geomean_range(c, 0, rows_.size());  }

   //Geomean over every ratio cell of the table (all rows, all non-baseline
   //columns; the baseline column is excluded since it is always 1.0).
   double general_geomean() const
   {
      double      log_sum = 0.0;
      std::size_t count   = 0;
      for(std::size_t i = 0; i < rows_.size(); ++i) {
         const row& r = rows_[i];
         const double base = r.sec.empty() ? 0.0 : r.sec[0];
         if(base > 0.0) {
            for(std::size_t c = 1; c < r.sec.size(); ++c) {
               if(r.sec[c] > 0.0) { log_sum += std::log(r.sec[c] / base); ++count; }
            }
         }
      }
      return count ? std::exp(log_sum / double(count)) : 0.0;
   }

   bc::string                 elem_name_;
   bool                       prereserve_;
   bc::vector<bc::string>     cols_;
   bc::vector<row>            rows_;
};

///////////////////////////////////////////////////////////////////////////////
// runner: the customization point's argument. A runner registers each container
// with add<Container>(name); the FIRST one registered is the baseline. add()
// measures the container across the whole size sweep; flush() (called by the
// harness) appends the rows to the shared report.
///////////////////////////////////////////////////////////////////////////////
inline std::size_t bench_num_sizes()
{  return bench_vector_defaults::max_size_exp - bench_vector_defaults::min_size_exp + 1u; }

template<class IntType, class Operation>
class runner
{
   public:
   runner(report& rep, bool prereserve)
      : rep_(rep), prereserve_(prereserve), sec_by_size_(bench_num_sizes())
   {}

   template<class Container>
   void add(const char* name)
   {
      using namespace bench_vector_detail;
      names_.push_back(name);
      for(std::size_t si = 0; si < bench_num_sizes(); ++si) {
         const std::size_t exp = bench_vector_defaults::min_size_exp + si;
         const std::size_t n   = bench_pow10(exp);
         const double sec = measure(
            container_build<Container>(n, prereserve_),
            bench_vector_defaults::num_trials,
            bench_vector_defaults::min_time_per_trial);
         sec_by_size_[si].push_back(sec);
      }
   }

   void flush()
   {
      if(!rep_.has_columns())
         rep_.set_columns(names_);
      for(std::size_t si = 0; si < bench_num_sizes(); ++si) {
         const std::size_t exp   = bench_vector_defaults::min_size_exp + si;
         const std::size_t n     = bench_pow10(exp);
         Operation         op(n);
         const std::size_t mult  = op.capacity_multiplier();
         const std::size_t n_eff = mult ? (n / mult) * mult : 0u;
         rep_.add_row(op.name(), exp, n_eff, sec_by_size_[si]);
      }
   }

   private:
   //Callable adapter so measure() (which expects f()) can build a Container.
   template<class Container>
   struct container_build
   {
      std::size_t n;
      bool        prereserve;
      container_build(std::size_t n_, bool pr) : n(n_), prereserve(pr) {}
      unsigned operator()() const
      {  return bench_vector_detail::build_once<Container, Operation>(n, prereserve);  }
   };

   report&                          rep_;
   bool                             prereserve_;
   bc::vector<bc::string>           names_;
   bc::vector<bc::vector<double> >  sec_by_size_;   //[size_index][column]
};

//Customization point implemented by every runner: register (with add<>()) each
//container it wants to compare. The first one registered is the baseline.
template<class IntType, class Operation>
void run_containers(runner<IntType, Operation>& r);

///////////////////////////////////////////////////////////////////////////////
// Drivers.
///////////////////////////////////////////////////////////////////////////////
template<class IntType, class Operation>
void add_operation(report& rep, bool prereserve)
{
   runner<IntType, Operation> r(rep, prereserve);
   run_containers<IntType, Operation>(r);
   r.flush();
}

template<class IntType>
void test_vectors_pass(bool prereserve)
{
   report rep(bench_type_name<IntType>(), prereserve);

   //end
   add_operation<IntType, push_back<IntType> >(rep, prereserve);
   #if BOOST_CXX_VERSION >= 201103L
   add_operation<IntType, emplace_back<IntType> >(rep, prereserve);
   #endif
   add_operation<IntType, insert_end_range<IntType> >(rep, prereserve);
   add_operation<IntType, insert_end_repeated<IntType> >(rep, prereserve);

   //near end
   add_operation<IntType, insert_near_end<IntType> >(rep, prereserve);
   #if BOOST_CXX_VERSION >= 201103L
   add_operation<IntType, emplace_near_end<IntType> >(rep, prereserve);
   #endif
   add_operation<IntType, insert_near_end_range<IntType> >(rep, prereserve);
   add_operation<IntType, insert_near_end_rpt<IntType> >(rep, prereserve);

   rep.print();
}

template<class IntType>
void test_vectors()
{
   std::cout << "Benchmark config: sizes 1.E" << bench_vector_defaults::min_size_exp
             << " .. 1.E" << bench_vector_defaults::max_size_exp
             << ", trials " << bench_vector_defaults::num_trials
             << ", min " << (bench_vector_defaults::min_time_per_trial / 1000000)
             << " ms/trial\n";

   if(bench_vector_defaults::run_noreserve)
      test_vectors_pass<IntType>(false);
   if(bench_vector_defaults::run_prereserve)
      test_vectors_pass<IntType>(true);
}

#endif   //BOOST_CONTAINER_BENCH_VECTOR_COMMON_HPP

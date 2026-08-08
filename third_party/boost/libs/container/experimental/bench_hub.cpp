/* Benchmark of boost::container::hub against boost::container::nest
 * (and optionally plf::hive). Built on the shared node-container harness.
 *
 * Copyright 2026 Joaquin M Lopez Munoz.
 * Copyright 2026 Ion Gaztanaga.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#include <boost/config.hpp>

#if BOOST_CXX_VERSION < 202002L

int main() { return 0; }

#else

#ifdef NDEBUG
//#define LONG_BENCH
#endif

//#define PLF_HIVE_BENCH

#include "bench_node_cont.hpp"

#include <boost/container/hub.hpp>
#include <boost/container/experimental/nest.hpp>

//Container-specific fast paths for the segmented containers benchmarked here.
//They are declared in namespace boost::container so the harness' unqualified
//calls (erase_void / quick_emplace / quick_erase) find them through ADL and
//prefer them over the generic primary templates in bench_node_cont.hpp.
namespace boost {
namespace container {

//erase_void: hub/nest can erase without computing the next iterator.
template<typename... Args, typename Iterator>
BOOST_CONTAINER_FORCEINLINE void erase_void(hub<Args...>& x, Iterator it)
{
   x.erase_void(it);
}

template<typename... Args, typename Iterator>
BOOST_CONTAINER_FORCEINLINE void erase_void(nest<Args...>& x, Iterator it)
{
   x.erase_void(it);
}

//quick_emplace: nest has a faster, capacity-rollback-free insertion path.
template<typename... Args, typename T>
BOOST_CONTAINER_FORCEINLINE typename nest<Args...>::iterator
quick_emplace(nest<Args...>& x, const T& v)
{
   return x.quick_emplace(v);
}

//quick_erase: mirrors erase_void for the quick build path.
template<typename... Args, typename Iterator>
BOOST_CONTAINER_FORCEINLINE void quick_erase(hub<Args...>& x, Iterator it)
{
   x.erase_void(it);
}

template<typename... Args, typename Iterator>
BOOST_CONTAINER_FORCEINLINE void quick_erase(nest<Args...>& x, Iterator it)
{
   x.erase_void(it);
}

}  //namespace container
}  //namespace boost

//Selects the two containers compared by the harness. "num" is the numerator of
//the printed time ratios, "den" the denominator. The commented alternatives
//mirror the experiments previously inlined in this file.
struct config : bench_defaults
{
#if defined(PLF_HIVE_BENCH)
   template<class E> using num = plf::hive<E>;
   static constexpr const char* num_name = "plf::hive";
#else
   template<class E> using num = boost::container::hub<E>;
   static constexpr const char* num_name = "hub";
   //template<class E> using num = boost::container::nest<E>;
   //template<class E> using num = boost::container::nest<E, void,
   //   boost::container::nest_options_t< boost::container::store_data_in_block<true> > >;
   //template<class E> using num = boost::container::nest<E, void,
   //   boost::container::nest_options_t< boost::container::prefetch<false> > >;
#endif

   template<class E> using den = boost::container::nest<E>;
   static constexpr const char* den_name = "nest";
   //template<class E> using den = boost::container::hub<E>;
   //template<class E> using den = boost::container::nest<E, void,
   //   boost::container::nest_options_t< boost::container::prefetch<false> > >;
   //template<class E> using den = boost::container::nest<E, void,
   //   boost::container::nest_options_t< boost::container::store_data_in_block<true> > >;
};

int main(int argc, char* argv[])
{
   (void)argc;
   (void)argv;
   return bench_main<config>();
}

#endif

/* Benchmark of boost::container::list against std::list. Built on the shared
 * node-container harness (bench_node_cont.hpp), reusing the same tests and
 * reporting used by bench_hub.cpp.
 *
 * Copyright 2026 Joaquin M Lopez Munoz.
 * Copyright 2026 Ion Gaztanaga.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#include <boost/config.hpp>

#if BOOST_CXX_VERSION < 201103L

int main() { return 0; }

#else

#ifdef _MSC_VER
    #if defined(_ITERATOR_DEBUG_LEVEL) && (_ITERATOR_DEBUG_LEVEL != 0)
        // You can choose to force-disable it:
        #undef _ITERATOR_DEBUG_LEVEL
        #define _ITERATOR_DEBUG_LEVEL 0
    #endif
#endif

#ifdef NDEBUG
//#define LONG_BENCH
#endif

#include "bench_node_cont.hpp"

#include <boost/container/list.hpp>
#include <list>

//Selects the two list containers compared by the harness. The shared harness
//uses push_back / erase / sort / range iteration for plain node containers, so
//both list types plug in without any extra customization.
struct config : bench_defaults
{
   template<class E> using num = boost::container::list<E>;
   template<class E> using den = std::list<E>;
   static constexpr const char* num_name = "boost::container::list";
   static constexpr const char* den_name = "std::list";
};

int main(int argc, char* argv[])
{
   (void)argc;
   (void)argv;
   return bench_main<config>();
}

#endif

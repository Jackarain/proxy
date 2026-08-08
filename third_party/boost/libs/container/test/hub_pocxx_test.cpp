/* Copyright 2025-2026 Joaquin M Lopez Munoz.
 * Copyright 2026 Ion Gaztanaga.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#include <boost/config.hpp>

#if BOOST_CXX_VERSION < 201103L

int main() { return 0; }

#else

#include <boost/config.hpp>
#include <boost/container/hub.hpp>
#include <boost/core/allocator_access.hpp>
#include <boost/core/lightweight_test.hpp>
#include <type_traits>
#include "hub_utility.hpp"

#if defined(BOOST_MSVC)
#pragma warning(push)
#pragma warning(disable:4127) /* conditional expression is constant */
#endif

template<typename Hub, typename Propagate, typename AlwaysEqual>
void test()
{
  using hub = rebind_allocator_t<
    Hub, stateful_allocator<void, Propagate,AlwaysEqual>>;
  using value_type = typename hub::value_type;
  using allocator_type = typename hub::allocator_type;
  static constexpr auto pocca =
    boost::allocator_propagate_on_container_copy_assignment_t<
      allocator_type>::value;
  static constexpr auto pocma =
    boost::allocator_propagate_on_container_move_assignment_t<
      allocator_type>::value;
  static constexpr auto pocs =
    boost::allocator_propagate_on_container_swap_t<allocator_type>::value;

  auto           rng = make_range<value_type>(200),
                 long_rng = make_range<value_type>(400);
  allocator_type al0{0}, al1{1};

  {
    hub  x(long_rng.begin(), long_rng.end(), al0),
         y(rng.begin(), rng.end(), al1);
    auto nx = x.get_allocator().num_allocations,
         ny = y.get_allocator().num_allocations;

    x = y;
    BOOST_TEST(x.get_allocator().state == (pocca? al1: al0).state);
    auto nx1 = x.get_allocator().num_allocations;
    BOOST_TEST(
      pocca && al0 == al1? nx1 == ny: 
      pocca && al0 != al1? nx1 >  ny : 
      /* !pocca */         nx1 == nx);
  }
  {
    hub  x(rng.begin(), rng.end(), al0),
         y(long_rng.begin(), long_rng.end(), al1);
    auto nx = x.get_allocator().num_allocations,
         ny = y.get_allocator().num_allocations;

    x = std::move(y);
    BOOST_TEST(x.get_allocator().state == (pocma? al1: al0).state);
    auto nx1 = x.get_allocator().num_allocations;
    BOOST_TEST(
      !pocma && al0 == al1? nx1 == nx:
      !pocma && al0 != al1? nx1 >  nx:
      /* pocma */           nx1 == ny);
  }
  if(pocs || al0 == al1) {
    hub  x(rng.begin(), rng.end(), al0),
         y(long_rng.begin(), long_rng.end(), al1);
    auto nx = x.get_allocator().num_allocations,
         ny = y.get_allocator().num_allocations;

    x.swap(y);
    BOOST_TEST(x.get_allocator().state == (pocs? al1: al0).state);
    BOOST_TEST(y.get_allocator().state == (pocs? al0: al1).state);
    auto nx1 = x.get_allocator().num_allocations;
    auto ny1 = y.get_allocator().num_allocations;
    BOOST_TEST(pocs? nx1 == ny: nx1 == nx);
    BOOST_TEST(pocs? ny1 == nx: ny1 == ny);
  }
}

#if defined(BOOST_MSVC)
#pragma warning(pop) /* C4127 */
#endif

template<typename Hub>
void test()
{
  test<Hub, std::false_type, std::false_type>();
  test<Hub, std::false_type, std::true_type >();
  test<Hub, std::true_type,  std::false_type>();
  test<Hub, std::true_type,  std::true_type >();
}

int main()
{
  test<boost::container::hub<int>>();

  return boost::report_errors();
}

#endif

/* Copyright 2026 Joaquin M Lopez Munoz.
 * Copyright 2026 Ion Gaztanaga.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#include <boost/config.hpp>

#if BOOST_CXX_VERSION < 201103L

int main() { return 0; }

#else

#include <algorithm>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>
#include <boost/container/hub.hpp>
#include <boost/core/detail/splitmix64.hpp>
#include <boost/core/lightweight_test.hpp>

struct big_nontrivial_int
{
  big_nontrivial_int(int n_ = 0): n{n_} {}
  big_nontrivial_int(const big_nontrivial_int& x): n{x.n} {}
  ~big_nontrivial_int() {}

  big_nontrivial_int& operator=(const big_nontrivial_int& x)
  {
    n = x.n;
    return *this;
  }

  operator int() const { return n; }

  int           n;
  unsigned char stuff[2 * sizeof(void*)];
};

static_assert(
  !std::is_trivially_destructible<big_nontrivial_int>::value
#if defined(BOOST_LIBSTDCXX_VERSION) && (BOOST_LIBSTDCXX_VERSION >= 50000)
  && !std::is_trivially_copy_constructible<big_nontrivial_int>::value
  && !std::is_trivially_assignable<
    big_nontrivial_int, big_nontrivial_int>::value
#endif
  ,
  "internal check on big_nontrivial_int");

template<typename Hub, typename Compare = std::less<typename Hub::value_type>>
void test(std::size_t n, double erase_rate, Compare comp = Compare())
{
  using value_type = typename Hub::value_type;

  std::size_t   m = (std::size_t)((double)n / (1.0 - erase_rate));
  std::uint64_t erase_cut = 
                  (std::uint64_t)(erase_rate * (double)(std::uint64_t)(-1));
  Hub           h;

  boost::detail::splitmix64 rng;
  for(std::size_t i = 0; i < m; ++i) h.insert((int)rng());
  erase_if(h, [&](value_type&) { return rng() < erase_cut; });
  h.sort(comp);
  BOOST_TEST(std::is_sorted(h.begin(), h.end(), comp));
}

template<typename Hub>
void test()
{
  using value_type = typename Hub::value_type;
  constexpr std::size_t small_n = 1000,
                        large_n = 300000; /* enough to trigger compact_sort */

  test<Hub>(0, 0.0);
  test<Hub>(1, 0.0);

  for(double erase_rate: {0.0, 0.00001, 0.2, 0.8}) {
    test<Hub>(small_n, erase_rate);
    test<Hub>(large_n, erase_rate);
    test<Hub>(small_n, erase_rate, std::greater<value_type>{});
    test<Hub>(large_n, erase_rate, std::greater<value_type>{});
  }
}

int main()
{
  test<boost::container::hub<int>>();
  test<boost::container::hub<big_nontrivial_int>>();

  return boost::report_errors();
}

#endif

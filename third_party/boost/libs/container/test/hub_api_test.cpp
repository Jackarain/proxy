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

#include <algorithm>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>
#include <boost/container/hub.hpp>
#include <boost/container/pmr/hub.hpp>
#include <boost/container/throw_exception.hpp>
#include <boost/core/allocator_access.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/pointer_traits.hpp>

/* GCC on Darwin cannot parse the system <mach/message.h> header
 * (xnu_static_assert_struct_size uses Clang-only extensions).
 */
#if defined(__GNUC__) && !defined(__clang__) && defined(__APPLE__)
#define BOOST_CONTAINER_HUB_TEST_API_NO_INTERPROCESS
#endif

#if !defined(BOOST_NO_EXCEPTIONS) && !defined(BOOST_CONTAINER_HUB_TEST_API_NO_INTERPROCESS)
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#endif

#include <iterator>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include "hub_utility.hpp"

enum tracked_provenance { ab_ovo = 0, from_copy, from_move };

template<typename T>
struct tracked
{
  tracked(const T& x_): x{x_}, origin{from_copy} {}
  tracked(T&& x_): x{std::move(x_)}, origin{from_move} {}
  tracked(const tracked& x_): x{x_.x}, origin{x_.origin}, last_op{from_copy} {}
  tracked(tracked&& x_): 
    x{std::move(x_.x)}, origin{x_.origin}, last_op{from_move} {}

  tracked& operator=(const tracked& x_)
  {
    x = x_.x;
    origin = x_.origin;
    last_op = from_copy;
    return *this;
  }

  tracked& operator=(tracked&& x_)
  {
    x = x_.x;
    origin = x_.origin;
    last_op = from_move;
    return *this;
  }

  T x;
  tracked_provenance origin, last_op = ab_ovo;
};

template<typename Hub, typename... Args>
Hub noalloc_construct(
  std::true_type, const typename Hub::allocator_type&, Args&&... args)
{
  return Hub(std::forward<Args>(args)...);
}

template<typename Hub, typename... Args>
Hub noalloc_construct(
  std::false_type, const typename Hub::allocator_type& al, Args&&... args)
{
  return Hub(std::forward<Args>(args)..., al);
}

template<typename Hub, typename... Args>
Hub noalloc_construct(
  const typename Hub::allocator_type& al, Args&&... args)
{
  return noalloc_construct<Hub>(
    std::is_default_constructible<typename Hub::allocator_type>{},
    al, std::forward<Args>(args)...);
}

template<typename Container1, typename Container2>
void test_equal(const Container1& x, const Container2& y)
{
  BOOST_TEST_EQ(x.size(), y.size());
  BOOST_TEST(std::equal(x.begin(), x.end(), y.begin()));
}

template<typename Iterator, typename Mirror>
void test_traversal(Iterator first, Iterator last, const Mirror& data)
{
  std::size_t n = 0;
  for(auto it = first; it != last; ++it, ++n)
  {
    BOOST_TEST(*it == data[n]);
    BOOST_TEST((first == it) == (0 == n));
    BOOST_TEST((first != it) == (0 != n));

    auto it1 = it, it2 = ++it1, it3 = --it2, it4 = it3++, it5 = it3-- ;
    BOOST_TEST(it1 == std::next(it));
    BOOST_TEST(it2 == it);
    BOOST_TEST(it3 == it);
    BOOST_TEST(it4 == it);
    BOOST_TEST(it5 == std::next(it));
  }
}

template<typename Hub, typename R>
void test_global_erase(const R& rng, const typename Hub::allocator_type& al)
{
  using value_type = typename Hub::value_type;
  using size_type = typename Hub::size_type;

  Hub         x{al};
  auto        even = [](const value_type& v) { return (int)(v) % 2 == 0; };
  const auto& odd_value = *std::find_if_not(rng.begin(), rng.end(), even);

  BOOST_TEST_EQ(erase(x, odd_value), 0);
  BOOST_TEST_EQ(x.size(), 0);
  BOOST_TEST_EQ(erase_if(x, even), 0);
  BOOST_TEST_EQ(x.size(), 0);

  x.insert(rng.begin(), rng.end());
  auto s = x.size();
  auto n = erase(x, odd_value);
  BOOST_TEST_EQ(
    n, (size_type)std::count(rng.begin(), rng.end(), odd_value));
  BOOST_TEST_EQ(std::count(x.begin(), x.end(), odd_value), 0);
  BOOST_TEST_EQ(x.size(), s - n);
  s = x.size();
  n = erase_if(x, even);
  BOOST_TEST_EQ(
    n, (size_type)std::count_if(rng.begin(), rng.end(), even));
  BOOST_TEST_EQ(std::count_if(x.begin(), x.end(), even), 0);
  BOOST_TEST_EQ(x.size(), s - n);
}

template<typename T> void avoid_unused_local_typedef() {}

template<typename Hub>
void test(const typename Hub::allocator_type& al = {})
{
  using value_type = typename Hub::value_type;
  using allocator_type= typename Hub::allocator_type;
  using pointer = typename Hub::pointer;
  using const_pointer = typename Hub::const_pointer;
  using reference = typename Hub::reference;
  using const_reference = typename Hub::const_reference;
  using difference_type = typename Hub::difference_type;
  using iterator = typename Hub::iterator;
  using const_iterator = typename Hub::const_iterator;
  using reverse_iterator = typename Hub::reverse_iterator;
  using const_reverse_iterator = typename Hub::const_reverse_iterator;

  avoid_unused_local_typedef<allocator_type>();
  avoid_unused_local_typedef<pointer>();
  avoid_unused_local_typedef<const_pointer>();
  avoid_unused_local_typedef<reference>();
  avoid_unused_local_typedef<const_reference>();
  avoid_unused_local_typedef<difference_type>();
  avoid_unused_local_typedef<reverse_iterator>();
  avoid_unused_local_typedef<const_reverse_iterator>();

#if !defined(BOOST_NO_CXX20_HDR_CONCEPTS)
  static_assert(std::bidirectional_iterator<iterator>);
  static_assert(std::bidirectional_iterator<const_iterator>);
#endif

  auto                              rng = make_range<value_type>(200);
  std::initializer_list<value_type> il{rng[5], rng[1], rng[7]};
  std::vector<value_type>           zeros(70, value_type());
  std::vector<value_type>           repeated(100, rng[10]);

  /* construct/copy/destroy */

  {
    Hub x = noalloc_construct<Hub>(al), y{al};
    BOOST_TEST(x.empty());
    BOOST_TEST(y.empty());
  }
  {
    Hub x = noalloc_construct<Hub>(al, zeros.size()),
        y{zeros.size(), al};
    test_equal(x, zeros);
    test_equal(y, zeros);
  }
  {
    Hub x = noalloc_construct<Hub>(al, repeated.size(), repeated.front()),
        y{repeated.size(), repeated.front(), al};
    test_equal(x, repeated);
    test_equal(y, repeated);
  }
  {
    Hub x = noalloc_construct<Hub>(al, repeated.size(), repeated[0]),
        y{repeated.size(), repeated[0], al};
    test_equal(x, repeated);
    test_equal(y, repeated);
  }
  {
    /* [sequence.reqmts/69.1] */

    using hub2 = rebind_value_type_t<Hub, unsigned int>;

    hub2 x = noalloc_construct<hub2>(al, 20u, 20u);
    BOOST_TEST_EQ(x.size(), 20u);
  }
  {
    Hub x = noalloc_construct<Hub>(al, rng.begin(), rng.end()), 
        y{rng.begin(), rng.end(), al};
    test_equal(x, rng);
    test_equal(y, rng);
  }
#if !defined(BOOST_CONTAINER_HUB_NO_RANGES)
  {
    Hub x = noalloc_construct<Hub>(al, boost::container::from_range, rng),
        y{boost::container::from_range, rng, al};
    test_equal(x, rng);
    test_equal(y, rng);
  }
#endif
  {
    const Hub x{rng.begin(), rng.end(), al};
    Hub       y{x}, z{y, al};
    test_equal(x, y);
    test_equal(x, z);
  }
  {
    Hub x{rng.begin(), rng.end(), al};
    Hub y{std::move(x)};
    BOOST_TEST(x.empty());
    test_equal(y, rng);

    Hub z{std::move(y), al};
    BOOST_TEST(y.empty());
    test_equal(z, rng);
  }
  {
    /* move construction with unequal allocators */
    using hub2 = rebind_allocator_t<Hub, stateful_allocator<void>>;
    using allocator_type2 = typename hub2::allocator_type;

    hub2 x{rng.begin(), rng.end(), allocator_type2{0}},
         y{std::move(x), allocator_type2{1}};
    BOOST_TEST_EQ(x.get_allocator().state, 0);
    BOOST_TEST(x.empty());
    BOOST_TEST_EQ(y.get_allocator().state, 1);
    test_equal(y, rng);
  }
  {
    Hub x = noalloc_construct<Hub>(al, il), y{il, al};
    test_equal(x, il);
    test_equal(y, il);
  }
  {
    Hub  x{rng.begin(), rng.end(), al}, y = noalloc_construct<Hub>(al);
    Hub& ry = (y = x);
    BOOST_TEST_EQ(&ry, &y);
    test_equal(x, y);
  }
  {
    Hub  x{rng.begin(), rng.end(), al}, y = noalloc_construct<Hub>(al);
    Hub& ry = (y = std::move(x));
    BOOST_TEST_EQ(&ry, &y);
    BOOST_TEST(x.empty());
    test_equal(y, rng);
  }
  {
    Hub  x{al};
    Hub& rx = (x = il);
    BOOST_TEST_EQ(&rx, &x);
    test_equal(x, il);
  }
  {
    Hub x{rng.begin(), rng.begin() + (difference_type)(rng.size() / 2), al};
    puncture(x);
    x.assign(rng.begin(), rng.end());
    test_equal(x, rng);
  }
#if !defined(BOOST_CONTAINER_HUB_NO_RANGES)
  {
    Hub x(zeros.size(), al);
    x.assign_range(rng);
    test_equal(x, rng);
  }
#endif
  {
    Hub x(zeros.size(), al);
    puncture(x);
    x.assign(repeated.size(), repeated[0]);
    test_equal(x, repeated);
  }
  {
    Hub x(zeros.size(), al);
    puncture(x);
    x.assign(il);
    test_equal(x, il);
  }
  {
    const Hub x{al};
    BOOST_TEST(x.get_allocator() == al);
  }

  /* iterators */

  {
    auto       data = rng;
    Hub        x{data.begin(), data.end(), al};
    const Hub& cx=x;
    puncture(data);
    puncture(x);

    BOOST_TEST(x.rbegin().base() == x.end());
    BOOST_TEST(cx.rbegin().base() == cx.end());
    BOOST_TEST(x.rend().base() == x.begin());
    BOOST_TEST(cx.rend().base() == cx.begin());
    BOOST_TEST(cx.cbegin() == cx.begin());
    BOOST_TEST(cx.cend() == cx.end());
    BOOST_TEST(cx.crbegin() == cx.rbegin());
    BOOST_TEST(cx.crend() == cx.rend());

    iterator       it = x.begin(), it2 = x.end();
    const_iterator cit = it;
    BOOST_TEST(cit == it);
    cit = it2;
    BOOST_TEST(cit == it2);
    it = it2;
    BOOST_TEST(it == it2);

    test_traversal(x.begin(), x.end(), data);
    test_traversal(x.cbegin(), x.cend(), data);
  }
  {
    /* operator-> */

    rebind_value_type_t<Hub, std::pair<int, int>> x(al);
    x.emplace(18, 42);
    BOOST_TEST_EQ(x.begin()->first, 18);
    BOOST_TEST_EQ(x.cbegin()->second, 42);
  }

  /* capacity */

  {
    Hub        x{al};
    const Hub& cx = x;

    x.reserve(1000);
    x.insert(rng.begin(), rng.end());
    BOOST_TEST(!cx.empty());
    BOOST_TEST_EQ(cx.size(), rng.size());
    BOOST_TEST_GT(cx.max_size(), 0u);
    BOOST_TEST_GE(cx.capacity(), 1000u);

    Hub x2 = x;
    x.shrink_to_fit();
    test_equal(x, x2);
    BOOST_TEST_EQ(cx.size(), rng.size());
    BOOST_TEST_GE(cx.capacity(), rng.size());

    auto c = cx.capacity();
    x.reserve(c + 1000);
    x.trim_capacity(c + 500);
    BOOST_TEST_LT(cx.capacity(), c + 1000);
    BOOST_TEST_GE(cx.capacity(), c + 500);
    x.trim_capacity();
    BOOST_TEST_EQ(cx.capacity(), c);
    test_equal(x, x2);

#ifndef BOOST_NO_EXCEPTIONS
    if(cx.max_size() < (typename Hub::size_type)(-1)) {
      BOOST_TEST_THROWS(
        x.reserve(cx.max_size() + 1), boost::container::length_error_t);
    }
#endif
  }

  /* available list partitioned in (non-empty)|(empty) */

  {
    static std::size_t N = 64; /* implementation defined */

    Hub x{N + 1, value_type(), al};
    x.reserve(10 * N);
    for(std::size_t i = 0; i < N; ++i) x.erase(x.begin());
    x.trim_capacity();
    BOOST_TEST_EQ(x.capacity(), N);

    x = Hub{2 * N + 1, value_type(), al};
    x.reserve(10 * N);
    x.erase(x.begin(), std::next(x.begin(), (int)(2 * N)));
    x.trim_capacity();
    BOOST_TEST_EQ(x.capacity(), N);

    x = Hub{3 * N, value_type(), al};
    x.reserve(10 * N);
    auto pos0 = x.begin(),
         pos1 = std::next(x.begin(), (int)(N)),
         pos2 = std::next(x.begin(), (int)(2 * N));
    x.erase(pos2, std::next(pos2, (int)(N / 2)));
    x.erase(pos1, std::next(pos1, (int)(N / 2)));
    x.erase(pos0, std::next(pos0, (int)(N / 2)));
    x.shrink_to_fit();
    BOOST_TEST_EQ(x.capacity(), 2 * N);
  }

  /* modifiers */

  using tracked_value_type = tracked<value_type>;
  using tracked_hub = rebind_value_type_t<Hub, tracked_value_type>;

  {
    tracked_hub     x{al};
    tracked_value_type v{value_type{}};

    auto it = x.emplace(v.x);
    BOOST_TEST(it->x == v.x);
    BOOST_TEST(it->origin == from_copy);

    v.x += value_type(1);
    it = x.emplace(std::move(v.x));
    BOOST_TEST(it->x == v.x);
    BOOST_TEST(it->origin == from_move);

    v.x += value_type(1);
    it = x.emplace_hint(x.cbegin(), v);
    BOOST_TEST(it->x == v.x);
    BOOST_TEST(it->last_op == from_copy);

    v.x += value_type(1);
    it = x.emplace_hint(x.cbegin(), std::move(v));
    BOOST_TEST(it->x == v.x);
    BOOST_TEST(it->last_op == from_move);

    v.x += value_type(1);
    it = x.insert(v);
    BOOST_TEST(it->x == v.x);
    BOOST_TEST(it->last_op == from_copy);

    v.x += value_type(1);
    it = x.insert(std::move(v.x));
    BOOST_TEST(it->x == v.x);
    BOOST_TEST(it->origin == from_move);

    v.x += value_type(1);
    it = x.insert(x.cbegin(), v);
    BOOST_TEST(it->x == v.x);
    BOOST_TEST(it->last_op == from_copy);

    v.x += value_type(1);
    it = x.insert(x.cbegin(), std::move(v.x));
    BOOST_TEST(it->x == v.x);
    BOOST_TEST(it->origin == from_move);
  }
  {
    Hub x{al};

    x.insert(il);
    test_equal(x, il);
  }
#if !defined(BOOST_CONTAINER_HUB_NO_RANGES)
  {
    Hub x{al};
    x.insert_range(rng);
    test_equal(x, rng);
    x.insert_range(rng);
    BOOST_TEST_EQ(x.size(), 2 * rng.size());
  }
#endif
  {
    Hub x{al};

    x.insert(rng.begin(), rng.begin());
    BOOST_TEST(x.empty());

    x.insert(rng.begin(), rng.end());
    test_equal(x, rng);
  }
  {
    /* boundary conditions in range assignment */
    Hub x{al};

    x.assign(1, 1);
    x.assign(0, 1);
    BOOST_TEST_EQ(x.size(), 0);

    x.assign(65, 1);
    x.assign(65, 1);
    BOOST_TEST_EQ(x.size(), 65);
  }
  {
    Hub x{rng.begin(), rng.end(), al};

    auto it = x.erase(x.cbegin());
    BOOST_TEST_EQ(x.size(), rng.size() - 1);
    BOOST_TEST(*it == rng[1]);

    it = x.erase(x.cend(), x.cend());
    BOOST_TEST_EQ(x.size(), rng.size() - 1);
    BOOST_TEST(it == x.cend());

    it = x.erase(std::prev(x.cend()), std::prev(x.cend()));
    BOOST_TEST_EQ(x.size(), rng.size() - 1);
    BOOST_TEST(it == std::prev(x.cend()));

    it = x.erase(
      std::next(x.cbegin(), (difference_type)(x.size() / 2)), x.cend());
    BOOST_TEST_EQ(x.size(), (difference_type)(rng.size() - 1) / 2);
    BOOST_TEST(it == x.cend());
  }
  {
    Hub x0{rng.begin(), rng.end(), al}, 
        y0{rng.begin(), rng.begin() + (difference_type)(rng.size() / 2), al},
        x = x0, y = y0;

    x.swap(x);
    test_equal(x, x0);

    swap(x, x);
    test_equal(x, x0);

    x.swap(y);
    test_equal(x, y0);
    test_equal(y, x0);

    swap(x, y);
    test_equal(x, x0);
    test_equal(y, y0);
  }
  {
    Hub x{rng.begin(), rng.end(), al};

    x.clear();
    BOOST_TEST(x.empty());
    x.clear();
    BOOST_TEST(x.empty());
  }

  /* hive operations */

  {
    Hub x{rng.begin(), rng.end(), al}, y{x};
    
    auto it = y.begin();
    y.reserve(y.capacity() + 100);
    x.splice(y);
    BOOST_TEST_EQ(x.size(), 2 * rng.size());
    BOOST_TEST(y.empty());
    BOOST_TEST_GE(y.capacity(), 100u);
    BOOST_TEST(*it == rng[0]);

    y.splice(std::move(x));
    BOOST_TEST(x.empty());
    BOOST_TEST_EQ(y.size(), 2 * rng.size());
    BOOST_TEST(*it == rng[0]);
  }
  {
    Hub x{al};
    for(const auto& v: rng) {
      x.insert(v);
      x.insert(v);
    }

    auto s = x.unique(std::equal_to<value_type>{});
    BOOST_TEST_EQ(s, rng.size());
    BOOST_TEST_EQ(x.size(), rng.size());
  }
  {
    Hub x{al};
    x.insert(rng.begin(), rng.end());
    x.insert(rng.begin(), rng.end());

    x.sort(std::less<value_type>{});
    BOOST_TEST(std::is_sorted(x.begin(), x.end()));

    x.sort(std::greater<value_type>{});
    BOOST_TEST(std::is_sorted(x.rbegin(), x.rend()));
  }
  {
    Hub        x{rng.begin(), rng.end(), al}, y{x};
    const Hub& cx=x;

    for(auto it = x.cbegin(); it != x.cend(); ++it) {
      auto p = boost::pointer_traits<const_pointer>::pointer_to(*it);
      BOOST_TEST(x.get_iterator(p) == it);
      BOOST_TEST(cx.get_iterator(p) == it);
    }
  }

  test_global_erase<Hub>(rng, al);

  /* visitation */

  {
    Hub        x{rng.begin(), rng.end(), al};
    const Hub& cx=x;
    puncture(x);

    unsigned int res = 0;
    auto         f = [&] (value_type& v) { res += (unsigned int)v;};
    auto         cf = [&] (const value_type& v) { res += (unsigned int)v;};

    for(std::size_t i = 0; i < x.size() / 2; ++i) {
      auto first = std::next(x.begin(), (int)i),
           last = std::prev(x.end(), (int)i);
      auto cfirst = std::next(x.cbegin(), (int)i),
           clast = std::prev(x.cend(), (int)i);

      res = 0;
      decltype(f) ret1 = boost::container::for_each(first, last, f);
      (void)ret1;
      auto res1 = res;
      res = 0;
      decltype(cf) ret2 = boost::container::for_each(cfirst, clast, cf);
      (void)ret2;
      auto res2 = res;
      res = 0;
      std::for_each(first, last, f);
      auto res3 = res;
      BOOST_TEST_EQ(res1, res3);
      BOOST_TEST_EQ(res2, res3);
    }

    res = 0;
    decltype(f) ret1 = for_each(x, f); 
    (void)ret1;
    auto res1 = res;
    res = 0;
    decltype(cf) ret2 = for_each(cx, cf);
    (void)ret2;
    auto res2 = res;
    res = 0;
    std::for_each(x.begin(), x.end(), f);
    auto res3 = res;
    BOOST_TEST_EQ(res1, res3);
    BOOST_TEST_EQ(res2, res3);
  }
  {
    Hub        x{rng.begin(), rng.end(), al};
    const Hub& cx=x;
    puncture(x);

    unsigned int res = 0;
    std::size_t  n = 0;
    auto         f = [&] (value_type& v) {
      if(!n--) return false;
      res += (unsigned int)v;
      return true;
    };
    auto         cf = [&] (const value_type& v) { 
      if(!n--) return false;
      res += (unsigned int)v;
      return true;
    };

    for(std::size_t i = 0; i <= x.size(); ++i) {
      auto first = std::next(x.begin(), (int)i);
      auto cfirst = std::next(x.cbegin(), (int)i);

      res = 0;
      n = (std::size_t)std::distance(first, x.end()) / 2;
      std::pair<iterator, decltype(f)> ret1 =
        boost::container::for_each_while(first, x.end(), f);
      auto it1 = ret1.first;
      auto res1 = res;
      res = 0;
      n = (std::size_t)std::distance(first, x.end()) / 2;
      std::pair<const_iterator, decltype(cf)> ret2 =
        boost::container::for_each_while(cfirst, cx.end(), cf);
      auto it2 = ret2.first;
      auto res2 = res;
      res = 0;
      n = (std::size_t)std::distance(first, x.end()) / 2;
      auto it3 = std::find_if_not(first, x.end(), f);
      auto res3 = res;
      BOOST_TEST(it1 == it3);
      BOOST_TEST_EQ(res1, res3);
      BOOST_TEST(it2 == it3);
      BOOST_TEST_EQ(res2, res3);
    }

    res = 0;
    n = x.size();
    std::pair<iterator, decltype(f)> ret1 = for_each_while(x, f);
    auto it1 = ret1.first;
    auto res1 = res;
    res = 0;
    n = x.size();
    std::pair<const_iterator, decltype(cf)> ret2 = for_each_while(cx, cf);
    auto it2 = ret2.first;
    auto res2 = res;
    res = 0;
    n = x.size();
    auto it3 = std::find_if_not(x.begin(), x.end(), f);
    auto res3 = res;
    BOOST_TEST(it1 == it3);
    BOOST_TEST_EQ(res1, res3);
    BOOST_TEST(it2 == it3);
    BOOST_TEST_EQ(res2, res3);
  }
}

template<template<typename...> class Hub>
void test_ctad()
{
#if !defined(BOOST_NO_CXX17_DEDUCTION_GUIDES) && \
    !BOOST_WORKAROUND(BOOST_CLANG_VERSION, < 90001)
  {
    std::vector<int> rng({0, 1, 2, 3});
    Hub              x1({0, 1, 2, 3});
    Hub              x2({0, 1, 2, 3}, std::allocator<int>{});
    Hub              x3(rng.begin(), rng.end());
    Hub              x4(rng.begin(), rng.end(), std::allocator<int>{});

    test_equal(x1, rng);
    test_equal(x2, rng);
    test_equal(x3, rng);
    test_equal(x4, rng);
  }
#if !defined(BOOST_CONTAINER_HUB_NO_RANGES)
  {
    std::vector<int> rng({0, 1, 2, 3});
    Hub x{boost::container::from_range, rng}; 
    Hub y{boost::container::from_range, rng, std::allocator<int>{}};
    test_equal(x, rng);
    test_equal(y, rng);
  }
#endif
#endif
}

#if !defined(BOOST_NO_EXCEPTIONS) && !defined(BOOST_CONTAINER_HUB_TEST_API_NO_INTERPROCESS)
#include <sstream>
const char *get_shared_memory_name()
{
   std::stringstream s;
   s << "process_" << boost::interprocess::ipcdetail::get_current_process_id();
   static std::string str = s.str();
   return str.c_str();
}
#endif

int main()
{
  test<boost::container::hub<int>>();
  test<boost::container::hub<std::size_t>>();

#if !defined(BOOST_NO_EXCEPTIONS) && !defined(BOOST_CONTAINER_HUB_TEST_API_NO_INTERPROCESS)
  namespace bip = boost::interprocess;
  using segment_manager = bip::managed_shared_memory::segment_manager;
  using shared_int_allocator = bip::allocator<int, segment_manager>;
  using shared_int_hub = boost::container::hub<int, shared_int_allocator>;

  static auto segment_name = get_shared_memory_name();
  static struct segment_remover {
    segment_remover() { bip::shared_memory_object::remove(segment_name); }
    ~segment_remover() { bip::shared_memory_object::remove(segment_name); }
  } remover; (void)remover;
  bip::managed_shared_memory segment(
    bip::create_only, segment_name, 64 * 1024);

  test<shared_int_hub>(shared_int_allocator(segment.get_segment_manager()));
#endif

  test<boost::container::pmr::hub<int>>();

  test_ctad<boost::container::hub>();

  return boost::report_errors();
}

#endif

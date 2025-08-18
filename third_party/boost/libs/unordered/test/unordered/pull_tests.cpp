// Copyright 2025 Joaquin M Lopez Munoz.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)


#include "../helpers/test.hpp"
#include "../helpers/unordered.hpp"
#include <algorithm>
#include <vector>

struct move_only_type
{
  move_only_type(int n_): n{n_} {}
  move_only_type(move_only_type&&) = default;
  move_only_type(const move_only_type&) = delete;

  move_only_type& operator=(move_only_type&&) = default;

  int n;
};

bool operator==(const move_only_type& x, const move_only_type& y)
{
  return x.n == y.n;
}

bool operator<(const move_only_type& x, const move_only_type& y)
{
  return x.n < y.n;
}

std::size_t hash_value(const move_only_type& x)
{
  return boost::hash<int>()(x.n);
}

template<typename T>
struct from_int
{
  T operator()(int n) const { return T(n); }
};

template<typename T, typename U>
struct from_int<std::pair<T, U> >
{
  std::pair<T, U> operator()(int n) const { return {n, -n}; }
};

template <class Container> void test_pull()
{
  Container c;
  using init_type = typename Container::init_type;

  std::vector<init_type> l1;
  from_int<init_type> fi;
  for(int i = 0; i < 1000; ++i ){
    l1.push_back(fi(i));
    c.insert(fi(i));
  }

  std::vector<init_type> l2;
  for(auto first = c.cbegin(), last = c.cend(); first != last; )
  {
    l2.push_back(c.pull(first++));
  }
  BOOST_TEST(c.empty());

  std::sort(l1.begin(), l1.end());
  std::sort(l2.begin(), l2.end());
  BOOST_TEST(l1 == l2);
}

UNORDERED_AUTO_TEST (pull_) {
#if defined(BOOST_UNORDERED_FOA_TESTS)
  test_pull<
    boost::unordered_flat_map<move_only_type, move_only_type> >();
  test_pull<
    boost::unordered_flat_set<move_only_type> >();
  test_pull<
    boost::unordered_node_map<move_only_type, move_only_type> >();
  test_pull<
    boost::unordered_node_set<move_only_type> >();
#else
  // Closed-addressing containers do not provide pull
#endif
}

RUN_TESTS()

// Copyright 2026 Braden Ganetsky
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#if !defined(BOOST_UNORDERED_FOA_TESTS)
#error "This test is only for the FOA-style conatiners"
#endif

#include <boost/static_assert.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <boost/unordered/unordered_node_map.hpp>
#include <boost/unordered/unordered_node_set.hpp>
// Don't include the CFOA headers here!

using flat_map = boost::unordered::unordered_flat_map<int, int>;
using flat_set = boost::unordered::unordered_flat_set<int>;
using node_map = boost::unordered::unordered_node_map<int, int>;
using node_set = boost::unordered::unordered_node_set<int>;

struct constrained_template_converter
{
  struct dummy
  {
  };
  template <class T, typename std::enable_if<
                       std::is_constructible<T, dummy>::value, int>::type = 0>
  operator T() const
  {
    return T{};
  }
};

// Check whether the corresponding CFOA container gets instantiated.
// The CFOA headers aren't included, so this would fail to compile if the CFOA
// container was instantiated.
BOOST_STATIC_ASSERT(
  (!std::is_constructible<flat_map, constrained_template_converter>::value));
BOOST_STATIC_ASSERT(
  (!std::is_constructible<flat_set, constrained_template_converter>::value));
BOOST_STATIC_ASSERT(
  (!std::is_constructible<node_map, constrained_template_converter>::value));
BOOST_STATIC_ASSERT(
  (!std::is_constructible<node_set, constrained_template_converter>::value));

int main() { return 0; }

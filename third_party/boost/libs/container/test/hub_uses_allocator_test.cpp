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

#include <boost/container/hub.hpp>
#include <boost/core/lightweight_test.hpp>
#include <scoped_allocator>
#include <string>
#include "hub_utility.hpp"

struct small_allocator_user
{
  using allocator_type = stateful_allocator<int>;

  small_allocator_user(const allocator_type& al_): al{al_} {}
  small_allocator_user(
    const small_allocator_user&, const allocator_type& al_): al{al_} {}

  operator int() const { return 0; }

  allocator_type al;
};

int main()
{
  {
    using string = std::basic_string<
      char, std::char_traits<char>, stateful_allocator<char>>;
    using hub = boost::container::hub<
      string, std::scoped_allocator_adaptor<stateful_allocator<string>>>;
    using allocator_type = hub::allocator_type;

    hub h(allocator_type(42));
    h.emplace("hello");
    BOOST_TEST(*h.begin() == "hello");
    BOOST_TEST_EQ(h.begin()->get_allocator().state, 42);
  }
  {
    using hub = boost::container::hub<
      small_allocator_user,
      std::scoped_allocator_adaptor<stateful_allocator<small_allocator_user>>>;
    using allocator_type = hub::allocator_type;

    hub h(10, small_allocator_user{allocator_type(0)}, allocator_type(42));
    h.sort();
    BOOST_TEST_EQ(h.begin()->al.state, 42);
  }

  return boost::report_errors();
}

#endif

// Copyright (C) 2021 Andrzej Krzemienski.
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/lib/optional for documentation.
//
// You are welcome to contact the author at:
//  akrzemi1@gmail.com

template <typename U>
struct wrapper {};

template <typename /*Tag*/, typename U>
int get(wrapper<U> const&) { return 0; }

#include "boost/optional/optional.hpp"

namespace boost
{
  struct any_type_in_boost_namespace {};
}

class tag; // user-defined tag

int main()
{
  // the following tests if boost::get for optional<> does
  // not interfere with the global get
  return get<tag>(wrapper<boost::any_type_in_boost_namespace>());
}

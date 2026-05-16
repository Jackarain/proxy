// Copyright (C) 2026, Andrzej Krzemieński.
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/lib/optional for documentation.
//
// You are welcome to contact the author at:
//  akrzemi1@gmail.com

#include "boost/optional.hpp"

#ifdef BOOST_OPTIONAL_USES_UNION_IMPLEMENTATION

struct Record
{
  int i;
  constexpr explicit Record(int i) : i(i) {}
  constexpr int operator()() const { return i; }
};

struct Guard
{
  int i;
  constexpr explicit Guard(int i) : i(i) {}
  Guard(Guard&&) = delete;
};

static_assert(boost::none == boost::none, "");
static_assert(!(boost::none != boost::none), "");

namespace test_int
{
  constexpr boost::optional<int> oN;
  static_assert(!oN, "");
  static_assert(oN == boost::none, "");
  static_assert(oN <= boost::none, "");
  static_assert(!(oN != boost::none), "");
  static_assert(oN == oN, "");
  static_assert(!oN.has_value(), "");
  static_assert(oN.value_or({}) == 0, "");
  static_assert(oN.value_or(0) == 0, "");
  static_assert(oN.value_or_eval(Record(9)) == 9, "");

  constexpr boost::optional<int> o1 (1);
  constexpr boost::optional<int> o2 {2};

  static_assert(o1, "");
  static_assert(o1.has_value(), "");
  static_assert(o1 != boost::none, "");
  static_assert(o1 != oN, "");
  static_assert(o1 > oN, "");
  static_assert(o1 >= oN, "");
  static_assert(*o1 == 1, "");
  static_assert(o1.value() == 1, "");
  static_assert(o1.value_or(0) == 1, "");
  static_assert(o1.value_or({}) == 1, "");
  static_assert(o1.value_or_eval(Record(9)) == 1, "");
  static_assert(o1 == 1, "");
  static_assert(o2, "");
  static_assert(o2 != o1, "");
  static_assert(o2 > o1, "");

#ifdef BOOST_OPTIONAL_CONSTEXPR_COPY
  constexpr boost::optional<int> oNc = oN;
  constexpr boost::optional<int> oNd = boost::none;
  constexpr boost::optional<int> oNe = {};
  static_assert(oNc == oN, "");
  static_assert(oNd == oN, "");
  static_assert(oNe == oN, "");

  constexpr bool test_reset() {
    boost::optional<int> o = 1;
    assert(o);
    o = boost::none;
    assert(!o);
    return o == boost::none;
  }

  static_assert(test_reset(), "");
#endif
}

namespace test_record
{
  constexpr boost::optional<Record> rN (boost::none);
  constexpr boost::optional<Record> r1 (Record(1));
  constexpr boost::optional<Record> r2 (boost::in_place_init, 2);

  static_assert(!rN, "");
  static_assert(rN == boost::none, "");
  static_assert(r1, "");
  static_assert(r1 != boost::none, "");
  static_assert(r2, "");
  static_assert(rN.value_or(Record(9)).i == 9, "");
  static_assert(r1->i == 1, "");
  static_assert(r2.value().i == 2, "");
}

namespace test_guard
{
  constexpr boost::optional<Guard> g1 {boost::in_place_init, 1};
  constexpr boost::optional<Guard> gNa {boost::none};
  constexpr boost::optional<Guard> gNb;

  static_assert(g1, "");
  static_assert(!!g1, "");
  static_assert(g1 != boost::none, "");
  static_assert(!(g1 == boost::none), "");
  static_assert(g1.has_value(), "");

  static_assert(!gNa, "");
  static_assert(!gNa.has_value(), "");
  static_assert(gNa == boost::none, "");
  static_assert(!(gNa != boost::none), "");

  static_assert(!gNb, "");
  static_assert(!gNb.has_value(), "");
  static_assert(gNb == boost::none, "");
  static_assert(!(gNb != boost::none), "");
}

namespace test_optional_ref
{
  constexpr int gi = 9;
  constexpr boost::optional<const int&> iref = gi;
  static_assert(iref, "");
  static_assert(*iref == 9, "");
}

#endif // BOOST_OPTIONAL_USES_UNION_IMPLEMENTATION

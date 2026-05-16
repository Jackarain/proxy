// Copyright (C) 2016 Andrzej Krzemienski.
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/lib/optional for documentation.
//
// You are welcome to contact the author at:
//  akrzemi1@gmail.com


#include "boost/optional/optional.hpp"

#ifdef BOOST_BORLANDC
#pragma hdrstop
#endif

#include "boost/core/lightweight_test.hpp"

using boost::optional;
using boost::none;

struct Value
{
  int val;
  BOOST_CONSTEXPR explicit Value(int v) : val(v) {}
};

struct Guard
{
  int val;
  BOOST_CONSTEXPR explicit Guard(int v) : val(v) {}
};

int val(int const& i)
{
  return i;
}

int val(Value const& v)
{
  return v.val;
}

int val(Guard const& v)
{
  return v.val;
}

template <typename Tref>
optional<Tref&> make_opt_ref(Tref& v)
{
  return optional<Tref&>(v);
}

template <typename Tval, typename Tref>
void test_construct_from_optional_ref()
{
    Tref v1 (1), v2 (2);

    optional<Tref&> opt_ref0;
    optional<Tref&> opt_ref1 (v1);

    optional<Tval> opt_val0 (opt_ref0);
    optional<Tval> opt_val1 (opt_ref1);
    optional<Tval> opt_val2 (make_opt_ref(v2));

    BOOST_TEST (!opt_val0);
    BOOST_TEST (opt_val1);
    BOOST_TEST (opt_val2);

    BOOST_TEST_EQ (1, val(*opt_val1));
    BOOST_TEST_EQ (2, val(*opt_val2));

    BOOST_TEST (boost::addressof(*opt_val1) != boost::addressof(v1));
    BOOST_TEST (boost::addressof(*opt_val2) != boost::addressof(v2));
}

template <typename Tval, typename Tref>
void test_assign_from_optional_ref()
{
    Tref v1 (1), v2 (2);

    optional<Tref&> opt_ref0;
    optional<Tref&> opt_ref1 (v1);

    optional<Tval> opt_val0;
    optional<Tval> opt_val1;
    optional<Tval> opt_val2;

    opt_val0 = opt_ref0;
    opt_val1 = opt_ref1;
    opt_val2 = make_opt_ref(v2);

    BOOST_TEST (!opt_val0);
    BOOST_TEST (opt_val1);
    BOOST_TEST (opt_val2);

    BOOST_TEST_EQ (1, val(*opt_val1));
    BOOST_TEST_EQ (2, val(*opt_val2));

    BOOST_TEST (boost::addressof(*opt_val1) != boost::addressof(v1));
    BOOST_TEST (boost::addressof(*opt_val2) != boost::addressof(v2));
}

template <typename T>
void test_convert_optional_T_to_optional_T_ref()
{
#ifdef BOOST_OPTIONAL_USES_UNION_IMPLEMENTATION
  using boost::optional;
  using boost::in_place_init;

  { // optional<T>& -> optional<T&>
    optional<T> ovN, ov1(in_place_init, 1), ov2(in_place_init, 2);

    optional<T&> orN = ovN;
    optional<T&> or1 = ov1;
    optional<T&> or2 = ov2;

    BOOST_TEST_EQ (!!orN, !!ovN);
    BOOST_TEST_EQ (!!or1, !!ov1);
    BOOST_TEST_EQ (!!or2, !!ov2);

    BOOST_TEST (or1);
    BOOST_TEST (or2);
    BOOST_TEST_EQ (val(*or1), 1);
    BOOST_TEST_EQ (val(*or2), 2);
    BOOST_TEST (boost::addressof(*or1) == boost::addressof(*ov1));
    BOOST_TEST (boost::addressof(*or2) == boost::addressof(*ov2));
  }

  { // const optional<T>& -> optional<const T&>
    constexpr optional<T> ovN;
    constexpr optional<T> ov1(in_place_init, 1);
    constexpr optional<T> ov2(in_place_init, 2);

    optional<const T&> orN = ovN;
    optional<const T&> or1 = ov1;
    optional<const T&> or2 = ov2;

    BOOST_TEST_EQ (!!orN, !!ovN);
    BOOST_TEST_EQ (!!or1, !!ov1);
    BOOST_TEST_EQ (!!or2, !!ov2);

    BOOST_TEST (or1);
    BOOST_TEST (or2);
    BOOST_TEST_EQ (val(*or1), 1);
    BOOST_TEST_EQ (val(*or2), 2);
    BOOST_TEST (boost::addressof(*or1) == boost::addressof(*ov1));
    BOOST_TEST (boost::addressof(*or2) == boost::addressof(*ov2));
  }

  { // optional<const T>& -> optional<const T&>
    optional<const T> ovN;
    optional<const T> ov1(in_place_init, 1);
    optional<const T> ov2(in_place_init, 2);

    optional<const T&> orN = ovN;
    optional<const T&> or1 = ov1;
    optional<const T&> or2 = ov2;

    BOOST_TEST_EQ (!!orN, !!ovN);
    BOOST_TEST_EQ (!!or1, !!ov1);
    BOOST_TEST_EQ (!!or2, !!ov2);

    BOOST_TEST (or1);
    BOOST_TEST (or2);
    BOOST_TEST_EQ (val(*or1), 1);
    BOOST_TEST_EQ (val(*or2), 2);
    BOOST_TEST (boost::addressof(*or1) == boost::addressof(*ov1));
    BOOST_TEST (boost::addressof(*or2) == boost::addressof(*ov2));
  }

  { // optional<T>& -> optional<const T&>
    optional<T> ovN;
    optional<T> ov1(in_place_init, 1);
    optional<T> ov2(in_place_init, 2);

    optional<const T&> orN = ovN;
    optional<const T&> or1 = ov1;
    optional<const T&> or2 = ov2;

    BOOST_TEST_EQ (!!orN, !!ovN);
    BOOST_TEST_EQ (!!or1, !!ov1);
    BOOST_TEST_EQ (!!or2, !!ov2);

    BOOST_TEST (or1);
    BOOST_TEST (or2);
    BOOST_TEST_EQ (val(*or1), 1);
    BOOST_TEST_EQ (val(*or2), 2);
    BOOST_TEST (boost::addressof(*or1) == boost::addressof(*ov1));
    BOOST_TEST (boost::addressof(*or2) == boost::addressof(*ov2));
  }

#endif // BOOST_OPTIONAL_USES_UNION_IMPLEMENTATION
}


int main()
{
    test_construct_from_optional_ref<int, int>();
    test_construct_from_optional_ref<int, int const>();
    test_construct_from_optional_ref<int const, int const>();
    test_construct_from_optional_ref<int const, int>();

    test_construct_from_optional_ref<Value, Value>();
    test_construct_from_optional_ref<Value, Value const>();
    test_construct_from_optional_ref<Value const, Value const>();
    test_construct_from_optional_ref<Value const, Value>();

    test_assign_from_optional_ref<int, int>();
    test_assign_from_optional_ref<int, int const>();

    test_assign_from_optional_ref<Value, Value>();
    test_assign_from_optional_ref<Value, Value const>();

    test_convert_optional_T_to_optional_T_ref<int>();
    test_convert_optional_T_to_optional_T_ref<Value>();
    test_convert_optional_T_to_optional_T_ref<Guard>();

    return boost::report_errors();
}

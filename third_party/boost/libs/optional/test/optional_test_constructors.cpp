// Copyright (C) 2026 Andrzej Krzemienski.
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
#include "boost/core/lightweight_test.hpp"
#include <string>


using boost::optional;
using boost::none;

struct MyBool
{
  bool b;
  MyBool (bool b) : b(b) {}
  operator bool() const { return b; };
};

struct MyExplicitBool
{
  bool b;
  MyExplicitBool (bool b) : b(b) {}
  operator bool() const { return b; };
};

struct Any
{
  template <typename... T>
  Any(T&&...) {}
};

struct JustCopyMoveCtor
{
  JustCopyMoveCtor(JustCopyMoveCtor&&) = default;
  JustCopyMoveCtor(JustCopyMoveCtor const&) = default;
  JustCopyMoveCtor& operator=(JustCopyMoveCtor&&) = delete;
  JustCopyMoveCtor& operator=(JustCopyMoveCtor const&) = delete;
  JustCopyMoveCtor() = delete;
};

struct JustDefault
{
  JustDefault() = default;
  JustDefault(JustDefault&&) = delete;
};

template <typename T>
optional<T> empty_optional()
{
    return optional<T>(); // this is to avoid "the most vexing parse"
}


template <typename T, typename U>
void direct_test_ctor_optional_T_from_empty_optional_T()
{
  optional<U> om;
  const optional<U> oc;
  BOOST_TEST(!om);
  BOOST_TEST(!oc);

  {
    optional<T> ox = om;
    BOOST_TEST(!ox);

    optional<T> oy = oc;
    BOOST_TEST(!oy);

    optional<T> oz = empty_optional<U>();
    BOOST_TEST(!oz);
  }

  {
    optional<T> ox = {om};
    BOOST_TEST(!ox);

    optional<T> oy = {oc};
    BOOST_TEST(!oy);

    optional<T> oz = {empty_optional<U>()};
    BOOST_TEST(!oz);
  }

  {
    optional<T> ox{om};
    BOOST_TEST(!ox);

    optional<T> oy{oc};
    BOOST_TEST(!oy);

    optional<T> oz{empty_optional<U>()};
    BOOST_TEST(!oz);
  }

  {
    optional<T> ox(om);
    BOOST_TEST(!ox);

    optional<T> oy(oc);
    BOOST_TEST(!oy);

    optional<T> oz(empty_optional<U>());
    BOOST_TEST(!oz);
  }
}

template <typename T, typename U>
void test_ctor_optional_T_from_empty_optional_T()
{
  direct_test_ctor_optional_T_from_empty_optional_T<T, U>();
  direct_test_ctor_optional_T_from_empty_optional_T<const T, const U>();
  direct_test_ctor_optional_T_from_empty_optional_T<optional<T>, optional<U>>();
  direct_test_ctor_optional_T_from_empty_optional_T<optional<const T>, optional<const U>>();
  direct_test_ctor_optional_T_from_empty_optional_T<const optional<T>, const optional<U>>();
}

template <typename T, typename U>
void direct_test_ctor_optional_T_from_optional_U(U val)
{
  optional<U> om;
  const optional<U> oc;
  BOOST_TEST(!om);
  BOOST_TEST(!oc);

  {
    optional<T> ox{om};
    BOOST_TEST(!ox);

    optional<T> oy{oc};
    BOOST_TEST(!oy);

    optional<T> oz{empty_optional<U>()};
    BOOST_TEST(!oz);
  }

  {
    optional<T> ox(om);
    BOOST_TEST(!ox);

    optional<T> oy(oc);
    BOOST_TEST(!oy);

    optional<T> oz(empty_optional<U>());
    BOOST_TEST(!oz);
  }

  {
    optional<U> om {val};
    const optional<U> oc {val};

    {
      optional<T> ox{om};
      BOOST_TEST(ox);

      optional<T> oy{oc};
      BOOST_TEST(oy);

      optional<T> oz{boost::make_optional(val)};
      BOOST_TEST(oz);
    }
    {
      optional<T> ox(om);
      BOOST_TEST(ox);

      optional<T> oy(oc);
      BOOST_TEST(oy);

      optional<T> oz(boost::make_optional(val));
      BOOST_TEST(oz);
    }
  }
}

template <typename T, typename U>
void test_ctor_optional_T_from_optional_U(U val)
{
  direct_test_ctor_optional_T_from_optional_U<T>(val);
}

template <typename T>
void direct_test_ctor_optional_T_from_valued_optional_T(T val)
{
  optional<T> om(val);
  const optional<T> oc(val);
  BOOST_TEST(om);
  BOOST_TEST(oc);

  {
    optional<T> ox = om;
    BOOST_TEST(ox);

    optional<T> oy = oc;
    BOOST_TEST(oy);

    optional<T> oz = optional<T>(val);
    BOOST_TEST(oz);
  }
  {
    optional<T> ox = {om};
    BOOST_TEST(ox);

    optional<T> oy = {oc};
    BOOST_TEST(oy);

    optional<T> oz = {optional<T>(val)};
    BOOST_TEST(oz);
  }
}

template <typename T>
void test_ctor_optional_T_from_valued_optional_T(T val)
{
  direct_test_ctor_optional_T_from_valued_optional_T<T>(val);
  direct_test_ctor_optional_T_from_valued_optional_T<optional<T>>(val);
  direct_test_ctor_optional_T_from_valued_optional_T<const T>(val);
  direct_test_ctor_optional_T_from_optional_U<T, T>(val);
  direct_test_ctor_optional_T_from_optional_U<optional<T>, optional<T>>(val);
}

template <typename T>
void direct_test_inplace_tag_constructor()
{
  {
    optional<T> ox (boost::in_place_init);
    BOOST_TEST(ox);
  }
  {
    optional<T> ox {boost::in_place_init};
    BOOST_TEST(ox);
  }
}

template <typename T>
void test_inplace_tag_constructor()
{
  direct_test_inplace_tag_constructor<T>();
  direct_test_inplace_tag_constructor<optional<T>>();
}


int main()
{
    test_ctor_optional_T_from_empty_optional_T<int, int>();
    test_ctor_optional_T_from_empty_optional_T<long, long>();
    test_ctor_optional_T_from_empty_optional_T<std::string, std::string>();
    test_ctor_optional_T_from_empty_optional_T<JustCopyMoveCtor, JustCopyMoveCtor>();
    test_ctor_optional_T_from_empty_optional_T<MyBool, MyBool>();

    test_ctor_optional_T_from_empty_optional_T<MyExplicitBool, MyExplicitBool>();

    test_ctor_optional_T_from_empty_optional_T<bool, bool>();
    test_ctor_optional_T_from_empty_optional_T<Any, Any>();

    test_inplace_tag_constructor<int>();
    test_inplace_tag_constructor<Any>();
    test_inplace_tag_constructor<JustDefault>();

    test_ctor_optional_T_from_optional_U<int>(1);
    test_ctor_optional_T_from_optional_U<long>(1);
    test_ctor_optional_T_from_optional_U<bool>(MyBool{true});
    test_ctor_optional_T_from_optional_U<bool>(MyBool{false});
    test_ctor_optional_T_from_optional_U<MyBool>(true);
    test_ctor_optional_T_from_optional_U<MyBool>(false);
    test_ctor_optional_T_from_optional_U<bool>(MyExplicitBool{true});
    test_ctor_optional_T_from_optional_U<bool>(MyExplicitBool{false});
    test_ctor_optional_T_from_optional_U<MyExplicitBool>(true);
    test_ctor_optional_T_from_optional_U<MyExplicitBool>(false);
    test_ctor_optional_T_from_optional_U<Any>(true);
    test_ctor_optional_T_from_optional_U<Any>(Any{});

    test_ctor_optional_T_from_valued_optional_T(1);

    optional<MyBool> ob;
    optional<bool> oa;
    oa = ob;
    BOOST_TEST(!oa);

    return boost::report_errors();
}

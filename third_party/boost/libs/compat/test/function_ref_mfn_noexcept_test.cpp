// Copyright 2024 Christian Mazakas
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#if defined(__GNUC__) && __GNUC__ == 7
#pragma GCC diagnostic ignored "-Wnoexcept-type"
#endif

#include <boost/compat/function_ref.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <memory>
#include <type_traits>

#if defined(__cpp_nontype_template_parameter_auto) && __cpp_nontype_template_parameter_auto >= 201606L

#define NONTYPE(fn) compat::nontype<fn>
#define NONTYPE_T(fn) compat::nontype_t<fn>

#else

#define NONTYPE(fn) compat::detail::nttp_holder<decltype(fn), fn>{}
#define NONTYPE_T(fn) compat::detail::nttp_holder<decltype(fn), fn>

#endif

struct F1 {
  std::unique_ptr<int> p_;

  F1() : p_(new int(1)) {}

  int m1() { return *p_ + -1; }
  int m2(int x1) noexcept { return 10 * *p_ + x1; }
  int m3(int x1, int x2) const { return 100 * *p_ + 10 * x1 + x2; }
  int m4(int x1, int x2, int x3) const noexcept { return 1000 * *p_ + 100 * x1 + 10 * x2 + x3; }
};

namespace compat = boost::compat;

int main() {
  {
#if defined(__cpp_noexcept_function_types)
    BOOST_TEST_TRAIT_FALSE(
        (std::is_constructible<compat::function_ref<int(F1&) noexcept>, NONTYPE_T(&F1::m1)>));
#endif
    BOOST_TEST_TRAIT_TRUE(
        (std::is_constructible<compat::function_ref<int(F1&, int) noexcept>, NONTYPE_T(&F1::m2)>));
#if defined(__cpp_noexcept_function_types)
    BOOST_TEST_TRAIT_FALSE(
        (std::is_constructible<compat::function_ref<int(F1&, int, int) noexcept>, NONTYPE_T(&F1::m3)>));
#endif
    BOOST_TEST_TRAIT_TRUE(
        (std::is_constructible<compat::function_ref<int(F1&, int, int, int) noexcept>, NONTYPE_T(&F1::m4)>));
  }

  {
    F1 f1;

    compat::function_ref<int(F1&, int) noexcept> fn2(NONTYPE(&F1::m2));
    BOOST_TEST_EQ(fn2(f1, 2), 12);

    compat::function_ref<int(F1&, int, int, int) noexcept> fn4(NONTYPE(&F1::m4));
    BOOST_TEST_EQ(fn4(f1, 2, 3, 4), 1234);
  }

  {
#if defined(__cpp_noexcept_function_types)
    BOOST_TEST_TRAIT_FALSE(
        (std::is_constructible<compat::function_ref<int(F1&) const noexcept>, NONTYPE_T(&F1::m1)>));
    BOOST_TEST_TRAIT_FALSE(
        (std::is_constructible<compat::function_ref<int(F1&, int, int) const noexcept>, NONTYPE_T(&F1::m3)>));
#endif

    F1 f1;

    compat::function_ref<int(F1&, int) const noexcept> fn2(NONTYPE(&F1::m2));
    BOOST_TEST_EQ(fn2(f1, 2), 12);

    compat::function_ref<int(F1&, int, int, int) const noexcept> fn4(NONTYPE(&F1::m4));
    BOOST_TEST_EQ(fn4(f1, 2, 3, 4), 1234);
  }

  {
    BOOST_TEST_TRAIT_FALSE(
        (std::is_constructible<compat::function_ref<int(F1 const&) noexcept>, NONTYPE_T(&F1::m1)>));
    BOOST_TEST_TRAIT_FALSE(
        (std::is_constructible<compat::function_ref<int(F1 const&, int) noexcept>, NONTYPE_T(&F1::m2)>));
#if defined(__cpp_noexcept_function_types)
    BOOST_TEST_TRAIT_FALSE(
        (std::is_constructible<compat::function_ref<int(F1 const&, int, int) noexcept>, NONTYPE_T(&F1::m3)>));
#endif

    F1 f1;

    compat::function_ref<int(F1 const&, int, int, int) noexcept> fn4(NONTYPE(&F1::m4));
    BOOST_TEST_EQ(fn4(f1, 2, 3, 4), 1234);
  }

  {
#if defined(__cpp_noexcept_function_types)
    BOOST_TEST_TRAIT_FALSE(
        (std::is_constructible<compat::function_ref<int() noexcept>, NONTYPE_T(&F1::m1), F1&>));
#endif
    BOOST_TEST_TRAIT_TRUE(
        (std::is_constructible<compat::function_ref<int(int) noexcept>, NONTYPE_T(&F1::m2), F1&>));
#if defined(__cpp_noexcept_function_types)
    BOOST_TEST_TRAIT_FALSE(
        (std::is_constructible<compat::function_ref<int(int, int) noexcept>, NONTYPE_T(&F1::m3), F1&>));
#endif
    BOOST_TEST_TRAIT_TRUE(
        (std::is_constructible<compat::function_ref<int(int, int, int) noexcept>, NONTYPE_T(&F1::m4), F1&>));

    F1 f1;

    compat::function_ref<int(int) noexcept> fn2(NONTYPE(&F1::m2), f1);
    BOOST_TEST_EQ(fn2(2), 12);

    compat::function_ref<int(int, int, int) noexcept> fn4(NONTYPE(&F1::m4), f1);
    BOOST_TEST_EQ(fn4(2, 3, 4), 1234);
  }

  {
    BOOST_TEST_TRAIT_FALSE(
        (std::is_constructible<compat::function_ref<int() const noexcept>, NONTYPE_T(&F1::m1), F1 const&>));
    BOOST_TEST_TRAIT_FALSE(
        (std::is_constructible<compat::function_ref<int(int) const noexcept>, NONTYPE_T(&F1::m2), F1 const&>));
#if defined(__cpp_noexcept_function_types)
    BOOST_TEST_TRAIT_FALSE((std::is_constructible<compat::function_ref<int(int, int) const noexcept>,
                                                  NONTYPE_T(&F1::m3), F1 const&>));
#endif

    F1 f1;

    compat::function_ref<int(int, int, int) const noexcept> fn4(NONTYPE(&F1::m4), f1);
    BOOST_TEST_EQ(fn4(2, 3, 4), 1234);

    auto const& f1_2 = f1;
    compat::function_ref<int(int, int, int) const noexcept> fn4_2(NONTYPE(&F1::m4), f1_2);
    BOOST_TEST_EQ(fn4_2(2, 3, 4), 1234);
  }

  {
#if defined(__cpp_noexcept_function_types)
    BOOST_TEST_TRAIT_FALSE(
        (std::is_constructible<compat::function_ref<int() noexcept>, NONTYPE_T(&F1::m1), F1*>));
#endif
    BOOST_TEST_TRAIT_TRUE(
        (std::is_constructible<compat::function_ref<int(int) noexcept>, NONTYPE_T(&F1::m2), F1*>));
#if defined(__cpp_noexcept_function_types)
    BOOST_TEST_TRAIT_FALSE(
        (std::is_constructible<compat::function_ref<int(int, int) noexcept>, NONTYPE_T(&F1::m3), F1*>));
#endif
    BOOST_TEST_TRAIT_TRUE(
        (std::is_constructible<compat::function_ref<int(int, int, int) noexcept>, NONTYPE_T(&F1::m4), F1*>));

    F1 f1;

    compat::function_ref<int(int) noexcept> fn2(NONTYPE(&F1::m2), &f1);
    BOOST_TEST_EQ(fn2(2), 12);

    compat::function_ref<int(int, int, int) noexcept> fn4(NONTYPE(&F1::m4), &f1);
    BOOST_TEST_EQ(fn4(2, 3, 4), 1234);
  }

  {
    BOOST_TEST_TRAIT_FALSE(
        (std::is_constructible<compat::function_ref<int() const noexcept>, NONTYPE_T(&F1::m1), F1 const*>));
    BOOST_TEST_TRAIT_FALSE(
        (std::is_constructible<compat::function_ref<int(int) const noexcept>, NONTYPE_T(&F1::m2), F1 const*>));
#if defined(__cpp_noexcept_function_types)
    BOOST_TEST_TRAIT_FALSE((std::is_constructible<compat::function_ref<int(int, int) const noexcept>,
                                                  NONTYPE_T(&F1::m3), F1 const*>));
#endif

    F1 const f1;

    compat::function_ref<int(int, int, int) const noexcept> fn4(NONTYPE(&F1::m4), &f1);
    BOOST_TEST_EQ(fn4(2, 3, 4), 1234);

    auto const& f1_2 = f1;
    compat::function_ref<int(int, int, int) const noexcept> fn4_2(NONTYPE(&F1::m4), &f1_2);
    BOOST_TEST_EQ(fn4_2(2, 3, 4), 1234);
  }

  return boost::report_errors();
}

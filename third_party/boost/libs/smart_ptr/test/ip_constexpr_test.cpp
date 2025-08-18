//
// ip_constexpr_test.cpp
//
// Copyright 2025 Mathias Stearn
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/intrusive_ptr.hpp>

#ifndef BOOST_SP_NO_CXX20_CONSTEXPR

struct dummy {
    // no-ops, so safe on pointers to static constexpr variables
    friend constexpr void intrusive_ptr_add_ref(const dummy *) {}
    friend constexpr void intrusive_ptr_release(const dummy *) {}
};
static constexpr dummy d;

struct subdummy : dummy {};

// Test that basic operations work at compile time.
static_assert(bool(boost::intrusive_ptr<const dummy>(&d)));
static_assert(!bool(boost::intrusive_ptr<dummy>(nullptr)));
static_assert(!bool(boost::intrusive_ptr<dummy>()));
static_assert(!bool(boost::intrusive_ptr<dummy>(boost::intrusive_ptr<subdummy>())));
static_assert(&*boost::intrusive_ptr<const dummy>(&d) == &d);
static_assert(boost::intrusive_ptr<const dummy>(&d).operator->() == &d);
static_assert(boost::intrusive_ptr<dummy>() == nullptr);
static_assert(boost::intrusive_ptr<dummy>() == boost::intrusive_ptr<dummy>(nullptr));
static_assert(boost::intrusive_ptr<dummy>() != boost::intrusive_ptr<const dummy>(&d));
static_assert(boost::intrusive_ptr<const dummy>(&d) != nullptr);
static_assert(boost::intrusive_ptr<const dummy>(&d) == boost::intrusive_ptr<const dummy>(&d));
static_assert(boost::intrusive_ptr<const dummy>(&d) == boost::intrusive_ptr<const dummy>(&d).get());
static_assert(boost::intrusive_ptr<const dummy>(&d) == boost::intrusive_ptr<const dummy>(&d).detach());
static_assert(!(boost::intrusive_ptr<const dummy>(&d) < boost::intrusive_ptr<const dummy>(&d)));
static_assert(boost::get_pointer(boost::intrusive_ptr<const dummy>(&d)) == &d);
static_assert(boost::static_pointer_cast<const dummy>( boost::intrusive_ptr<const dummy>(&d)) == &d);
static_assert(boost::const_pointer_cast<const dummy>( boost::intrusive_ptr<const dummy>(&d)) == &d);
static_assert(boost::dynamic_pointer_cast<const dummy>( boost::intrusive_ptr<const dummy>(&d)) == &d);

constexpr auto lvalue = boost::intrusive_ptr<const dummy>(&d);
constexpr auto lvalue_convertible = boost::intrusive_ptr<const subdummy>();
static_assert(boost::intrusive_ptr<const dummy>(lvalue) == &d);
static_assert(!boost::intrusive_ptr<const dummy>(lvalue_convertible));
static_assert(boost::static_pointer_cast<const dummy>(lvalue) == &d);
static_assert(boost::const_pointer_cast<const dummy>(lvalue) == &d);
static_assert(boost::dynamic_pointer_cast<const dummy>(lvalue) == &d);

// Works in places that static_assert doesn't, like expressions with
// non-constexpr variables in constexpr functions.
template <typename T> constexpr void semi_static_assert(T b) {
    if (!b)
        throw "assertion failed"; // Not constexpr so fails compile.
}

constexpr bool test_swap() {
    auto p1 = boost::intrusive_ptr<const dummy>(&d);
    auto p2 = boost::intrusive_ptr<const dummy>();
    swap(p1, p2);
    semi_static_assert(!p1 && p2);
    p1.swap(p2);
    semi_static_assert(p1 && !p2);
    return true;
}
static_assert(test_swap());

constexpr bool test_reset_assign() {
    // Test assignments resulting in nullptr
    {
        auto p1 = boost::intrusive_ptr<const dummy>(&d);
        p1.reset();
        semi_static_assert(!p1);
    }
    {
        auto p1 = boost::intrusive_ptr<const dummy>(&d);
        p1.reset(nullptr);
        semi_static_assert(!p1);
    }
    {
        auto p1 = boost::intrusive_ptr<const dummy>(&d);
        p1 = nullptr;
        semi_static_assert(!p1);
    }
    {
        auto p1 = boost::intrusive_ptr<const dummy>(&d);
        p1 = boost::intrusive_ptr<const dummy>();
        semi_static_assert(!p1);
    }
    {
        auto p1 = boost::intrusive_ptr<const dummy>(&d);
        p1 = boost::intrusive_ptr<subdummy>();
        semi_static_assert(!p1);
    }
    {
        auto p1 = boost::intrusive_ptr<const dummy>(&d);
        p1 = lvalue_convertible;
        semi_static_assert(!p1);
    }

    // Test assignments resulting in &d
    {
        auto p1 = boost::intrusive_ptr<const dummy>();
        p1.reset(&d);
        semi_static_assert(p1 == &d);
    }
    {
        auto p1 = boost::intrusive_ptr<const dummy>();
        p1.reset(&d, true);
        semi_static_assert(p1 == &d);
    }
    {
        auto p1 = boost::intrusive_ptr<const dummy>();
        p1 = boost::intrusive_ptr<const dummy>(&d);
        semi_static_assert(p1 == &d);
    }
    {
        auto p1 = boost::intrusive_ptr<const dummy>();
        p1 = lvalue;
        semi_static_assert(p1 == &d);
    }
    {
        auto p1 = boost::intrusive_ptr<const dummy>();
        p1 = &d;
        semi_static_assert(p1 == &d);
    }
    return true;
}
static_assert(test_reset_assign());

#endif
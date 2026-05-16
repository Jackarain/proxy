// Copyright 2026 Christian Granzin
// Copyright 2024 Christophe Henry
// henry UNDERSCORE christophe AT hotmail DOT com
// This is an extended version of the state machine available in the boost::mpl library
// Distributed under the same license as the original.
// Copyright for the original version:
// Copyright 2005 David Abrahams and Aleksey Gurtovoy. Distributed
// under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MSM_NONSTANDALONE_TEST
#define BOOST_TEST_MODULE backmp11_basic_polymorphic
#endif
#include <boost/test/unit_test.hpp>

#include <boost/msm/backmp11/detail/basic_polymorphic.hpp>
#include "Utils.hpp"

using boost::msm::backmp11::detail::basic_polymorphic;

namespace
{

struct trivial_class
{
    int data;
};

BOOST_AUTO_TEST_CASE(trivial_class_test)
{
    using polymorphic_t = basic_polymorphic<trivial_class>;
    static_assert(sizeof(polymorphic_t) == 64);

    polymorphic_t obj = polymorphic_t::make(trivial_class{42});
    BOOST_REQUIRE(obj->data == 42);

    // Copy
    {
        polymorphic_t obj_2{obj};
        BOOST_REQUIRE(obj_2->data == 42);

        polymorphic_t obj_3;
        obj_3 = obj;
        BOOST_REQUIRE(obj_3->data == 42);
    }

    // Move
    {
        polymorphic_t obj_2{std::move(obj)};
        BOOST_REQUIRE(obj_2->data == 42);

        polymorphic_t obj_3;
        obj_3 = std::move(obj);
        BOOST_REQUIRE(obj_3->data == 42);
    }
}

struct class_with_custom_operators
{
    class_with_custom_operators(int value) : value(value) {}

    class_with_custom_operators(const class_with_custom_operators& other)
    {
        value = other.value;
        copy_constructor_calls++;
    }

    class_with_custom_operators(class_with_custom_operators&& other) noexcept
    {
        value = other.value;
        move_constructor_calls++;
    }

    ~class_with_custom_operators()
    {
        destructor_calls++;
    }

    static inline size_t destructor_calls;
    static inline size_t copy_constructor_calls;
    static inline size_t move_constructor_calls;

    int value;
};

BOOST_AUTO_TEST_CASE(class_with_custom_operators_test)
{
    using polymorphic_t = basic_polymorphic<class_with_custom_operators>;

    size_t& destructor_calls = class_with_custom_operators::destructor_calls;
    size_t& move_constructor_calls = class_with_custom_operators::move_constructor_calls;
    size_t& copy_constructor_calls = class_with_custom_operators::copy_constructor_calls;

   {
        polymorphic_t obj = polymorphic_t::make<class_with_custom_operators>(42);
        BOOST_REQUIRE(obj->value == 42);
        BOOST_REQUIRE(obj.is_inline());

        polymorphic_t obj_2;
        obj_2 = obj;
        BOOST_REQUIRE(obj_2->value == 42);
        ASSERT_ONE_AND_RESET(copy_constructor_calls);
        
        obj_2 = std::move(obj);
        BOOST_REQUIRE(obj_2->value == 42);
        ASSERT_ONE_AND_RESET(move_constructor_calls);
    }
    // 2..3 destructor calls:
    // 1. obj's object destroyed at end of scope
    // 2. obj_2's object destroyed at end of scope
    // 3. MSVC doesn't apply copy elision when built in debug mode.
    BOOST_REQUIRE(destructor_calls == 2 || destructor_calls == 3);
    destructor_calls = 0;

    {
        polymorphic_t obj = polymorphic_t::make(class_with_custom_operators{42});
        BOOST_REQUIRE(obj->value == 42);
    }
    // 2..3 destructor calls:
    // 1. Temporary object destroyed after being moved into obj
    // 2. obj's object destroyed at end of scope
    // 3. GCC<14 cannot apply copy elision
    BOOST_REQUIRE(destructor_calls == 2 || destructor_calls == 3);
    destructor_calls = 0;
}

struct class_with_throwing_move_constructor
{
    class_with_throwing_move_constructor(uint8_t value) : value(value)
    {
    }

    class_with_throwing_move_constructor(const class_with_throwing_move_constructor&) = default;

    class_with_throwing_move_constructor(class_with_throwing_move_constructor&& other)
    {
        value = other.value;
    }

    ~class_with_throwing_move_constructor()
    {
        destructor_calls++;
    }

    static inline size_t destructor_calls;

    uint8_t value;
};

BOOST_AUTO_TEST_CASE(class_with_throwing_move_constructor_test)
{
    using polymorphic_t = basic_polymorphic<class_with_throwing_move_constructor>;
    {
        polymorphic_t obj = polymorphic_t::make<class_with_throwing_move_constructor>(42);
        BOOST_REQUIRE(!obj.is_inline());
        BOOST_REQUIRE(obj->value == 42);

        polymorphic_t obj_2 = obj;
        BOOST_REQUIRE(obj_2->value == 42);

        polymorphic_t obj_3 = std::move(obj);
        BOOST_REQUIRE(obj_3->value == 42);
    }
    // Only two destructor calls, because the values are on the heap.
    ASSERT_AND_RESET(class_with_throwing_move_constructor::destructor_calls, 2);
}

struct big_class_with_destructor
{
    big_class_with_destructor(uint8_t value) : value(value)
    {
    }

    ~big_class_with_destructor()
    {
        destructor_calls++;
    }

    static inline size_t destructor_calls;

    uint8_t value;
    std::array<uint8_t, 128> data;
};

BOOST_AUTO_TEST_CASE(big_class_with_destructor_test)
{
    using polymorphic_t = basic_polymorphic<big_class_with_destructor>;
    big_class_with_destructor val{42};
    {
        polymorphic_t obj{val};
        BOOST_REQUIRE(!obj.is_inline());
        BOOST_REQUIRE(obj->value == 42);

        polymorphic_t obj_2 = obj;
        BOOST_REQUIRE(obj_2->value == 42);

        polymorphic_t obj_3 = std::move(obj);
        BOOST_REQUIRE(obj_3->value == 42);
    }
    // Only two destructor calls, because the values are on the heap.
    ASSERT_AND_RESET(big_class_with_destructor::destructor_calls, 2);
}

struct sub_class_with_destructor : class_with_custom_operators
{
    sub_class_with_destructor(int data) : class_with_custom_operators(data) {}

    sub_class_with_destructor(const sub_class_with_destructor& other) = default;
    sub_class_with_destructor(sub_class_with_destructor&& other) noexcept = default;

    ~sub_class_with_destructor()
    {
        destructor_calls++;
    }

    static inline size_t destructor_calls;

    int data;
};

BOOST_AUTO_TEST_CASE(two_destructors_test)
{
    using polymorphic_t = basic_polymorphic<class_with_custom_operators>;

    [[maybe_unused]] size_t& destructor_calls_0 = class_with_custom_operators::destructor_calls;
    [[maybe_unused]] size_t& destructor_calls_1 = sub_class_with_destructor::destructor_calls;

    {
        polymorphic_t obj = polymorphic_t::make<sub_class_with_destructor>(42);
        BOOST_REQUIRE(obj->value == 42);
        BOOST_REQUIRE(obj.is_inline());
    }
    // 1..2 destructor calls:
    // 1. obj's object destroyed at end of scope
    // 2. MSVC doesn't apply copy elision when built in debug mode.
    BOOST_REQUIRE(class_with_custom_operators::destructor_calls == 1 ||
                  class_with_custom_operators::destructor_calls == 2);
    class_with_custom_operators::destructor_calls = 0;
    BOOST_REQUIRE(sub_class_with_destructor::destructor_calls == 1 ||
                  sub_class_with_destructor::destructor_calls == 2);
    sub_class_with_destructor::destructor_calls = 0;

    {
        polymorphic_t obj = polymorphic_t::make(sub_class_with_destructor{42});
        BOOST_REQUIRE(obj->value == 42);
    }
    // 2..3 destructor calls:
    // 1. Temporary object destroyed after being moved into obj
    // 2. obj's object destroyed at end of scope
    // 3. GCC<14 cannot apply copy elision.
    BOOST_REQUIRE(class_with_custom_operators::destructor_calls == 2 ||
                  class_with_custom_operators::destructor_calls == 3);
    class_with_custom_operators::destructor_calls = 0;
    BOOST_REQUIRE(sub_class_with_destructor::destructor_calls == 2 ||
                  sub_class_with_destructor::destructor_calls == 3);
    sub_class_with_destructor::destructor_calls = 0;
}

struct virtual_class
{
    virtual void method()
    {
        method_calls++;
    }

    static inline size_t method_calls;
};

struct other_virtual_class : virtual_class
{
    void method() override
    {
        method_calls++;
    }

    static inline size_t method_calls;
};

BOOST_AUTO_TEST_CASE(virtual_class_test)
{
    using polymorphic_t = basic_polymorphic<virtual_class>;

    polymorphic_t obj = polymorphic_t::make<virtual_class>();
    BOOST_REQUIRE(obj.is_inline());
    obj->method();
    ASSERT_AND_RESET(virtual_class::method_calls, 1);
    
    polymorphic_t other_obj = polymorphic_t::make<other_virtual_class>();
    obj = std::move(other_obj);
    obj->method();
    ASSERT_AND_RESET(other_virtual_class::method_calls, 1);
}

} // namespace

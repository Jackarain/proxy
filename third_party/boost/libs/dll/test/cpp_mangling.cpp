// Copyright 2024 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// For more information, see http://www.boost.org

#include <boost/core/lightweight_test.hpp>

#include <boost/dll/detail/demangling/msvc.hpp>

int main() {
    namespace parser = boost::dll::detail::parser;

    BOOST_TEST(parser::is_destructor_with_name("foo::~foo(void)")("public: __cdecl foo::~foo(void)"));
    BOOST_TEST(parser::is_destructor_with_name("foo::~foo(void)")("private: __cdecl foo::~foo(void)"));
    BOOST_TEST(parser::is_destructor_with_name("foo::~foo(void)")("protected: __cdecl foo::~foo(void)"));

    BOOST_TEST(parser::is_destructor_with_name("foo::~foo(void)")("public: virtual __cdecl foo::~foo(void)"));
    BOOST_TEST(parser::is_destructor_with_name("foo::~foo(void)")("private: virtual __cdecl foo::~foo(void)"));
    BOOST_TEST(parser::is_destructor_with_name("foo::~foo(void)")("protected: virtual __cdecl foo::~foo(void)"));

    BOOST_TEST(parser::is_destructor_with_name("foo::~foo(void)")("public: __cdecl foo::~foo(void) __ptr64"));
    BOOST_TEST(parser::is_destructor_with_name("foo::~foo(void)")("private: __cdecl foo::~foo(void) __ptr64"));
    BOOST_TEST(parser::is_destructor_with_name("foo::~foo(void)")("protected: __cdecl foo::~foo(void) __ptr64"));

    BOOST_TEST(parser::is_destructor_with_name("some_space::some_father::~some_father(void)")("public: __cdecl some_space::some_father::~some_father(void) __ptr64"));

    BOOST_TEST(parser::is_destructor_with_name("foo::~foo(void)")("public: __thiscall foo::~foo(void)"));
    BOOST_TEST(parser::is_destructor_with_name("foo::~foo(void)")("private: __thiscall foo::~foo(void)"));
    BOOST_TEST(parser::is_destructor_with_name("foo::~foo(void)")("protected: __thiscall foo::~foo(void)"));

    BOOST_TEST(parser::is_destructor_with_name("foo::~foo(void)")("public: virtual __thiscall foo::~foo(void)"));
    BOOST_TEST(parser::is_destructor_with_name("foo::~foo(void)")("private: virtual __thiscall foo::~foo(void)"));
    BOOST_TEST(parser::is_destructor_with_name("foo::~foo(void)")("protected: virtual __thiscall foo::~foo(void)"));

    BOOST_TEST(parser::is_destructor_with_name("foo::~foo(void)")("public: __thiscall foo::~foo(void) __ptr32"));
    BOOST_TEST(parser::is_destructor_with_name("foo::~foo(void)")("private: __thiscall foo::~foo(void) __ptr32"));
    BOOST_TEST(parser::is_destructor_with_name("foo::~foo(void)")("protected: __thiscall foo::~foo(void)  __ptr32"));

    BOOST_TEST(parser::is_destructor_with_name("some_space::some_father::~some_father(void)")("public: __thiscall some_space::some_father::~some_father(void) __ptr64"));


    boost::dll::detail::mangled_storage_impl ms;
    {
        void(*ptr0)(int) = nullptr;
        boost::core::string_view s = "integer";
        BOOST_TEST(parser::try_consume_arg_list(s, ms, ptr0));
        BOOST_TEST_EQ(s, "eger");
    }
    {
        void(*ptr1)(int) = nullptr;
        boost::core::string_view s = "int";
        BOOST_TEST(parser::try_consume_arg_list(s, ms, ptr1));
        BOOST_TEST(s.empty());
    }
    {
        void(*ptr2)() = nullptr;
        boost::core::string_view s = "void";
        BOOST_TEST(parser::try_consume_arg_list(s, ms, ptr2));
        BOOST_TEST(s.empty());
    }
    {
        void(*ptr3)(int,int) = nullptr;
        boost::core::string_view s = "int,int";
        BOOST_TEST(parser::try_consume_arg_list(s, ms, ptr3));
        BOOST_TEST(s.empty());
    }
    {
        void(*ptr4)(int,int,int) = nullptr;
        boost::core::string_view s = "int,int,int";
        BOOST_TEST(parser::try_consume_arg_list(s, ms, ptr4));
        BOOST_TEST(s.empty());
    }


    BOOST_TEST((
        parser::is_constructor_with_name<void(*)()>("some_space::some_class::some_class", ms)
            ("public: __cdecl some_space::some_class::some_class(void) __ptr64")
    ));
    BOOST_TEST((
        parser::is_constructor_with_name<void(*)(int)>("some_space::some_class::some_class", ms)
            ("private: __cdecl some_space::some_class::some_class(int)")
    ));
    BOOST_TEST((
        parser::is_constructor_with_name<void(*)(int,int)>("some_space::some_class::some_class", ms)
            ("private: __cdecl some_space::some_class::some_class(int,int)")
    ));


    BOOST_TEST((
        parser::is_variable_with_name<int>("some_space::some_class::value", ms)
            ("public: static int some_space::some_class::value")
    ));

    BOOST_TEST((
        parser::is_variable_with_name<int>("some_space::some_class::value", ms)
            ("int some_space::some_class::value")
    ));

    BOOST_TEST((
        parser::is_variable_with_name<double>("some_space::variable", ms)
            ("public: static double some_space::variable")
    ));

    BOOST_TEST((
        !parser::is_variable_with_name<double>("some_space::variable", ms)
            ("public: static int some_space::variable_that_is_not_exist")
    ));

    BOOST_TEST((
        parser::is_variable_with_name<const double>("unscoped_c_var", ms)
            ("double const unscoped_c_var")
    ));


    BOOST_TEST((
        parser::is_function_with_name<void(*)(int)>("overloaded", ms)
            ("void __cdecl overloaded(int)")
    ));

    BOOST_TEST((
        parser::is_function_with_name<void(*)(double)>("overloaded", ms)
            ("void __cdecl overloaded(double)")
    ));

    BOOST_TEST((
        parser::is_function_with_name<const int&(*)()>("some_space::scoped_fun", ms)
            ("int const & __ptr64 __cdecl some_space::scoped_fun(void)")
    ));


    BOOST_TEST((
        parser::is_mem_fn_with_name<volatile int, int(*)(int,int)>("func", ms)
            ("public: int __thiscall int::func(int,int)volatile ")
    ));
    BOOST_TEST((
        parser::is_mem_fn_with_name<const volatile int, double(*)(double)>("func", ms)
            ("private: double __thiscall int::func(double)const volatile ")
    ));

    BOOST_TEST((
        parser::is_mem_fn_with_name<volatile int, int(*)(int,int)>("func", ms)
            ("public: int __cdecl int::func(int,int)volatile __ptr64")
    ));

    BOOST_TEST((
        parser::is_mem_fn_with_name<volatile int, int(*)(int,int)>("func", ms)
            ("public: virtual int __cdecl int::func(int,int)volatile __ptr64")
    ));

    return boost::report_errors();
}


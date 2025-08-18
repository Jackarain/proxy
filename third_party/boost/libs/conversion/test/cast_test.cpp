//  boost utility cast test program  -----------------------------------------//

//  (C) Copyright Beman Dawes, Dave Abrahams 1999. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org for most recent version including documentation.

//  Revision History
//   28 Set 04  factored out numeric_cast<> test (Fernando Cacciola)
//   20 Jan 01  removed use of <limits> for portability to raw GCC (David Abrahams)
//   28 Jun 00  implicit_cast removed (Beman Dawes)
//   30 Aug 99  value_cast replaced by numeric_cast
//    3 Aug 99  Initial Version

#include <iostream>
#include <boost/polymorphic_cast.hpp>
#include <boost/core/lightweight_test.hpp>

using namespace boost;
using std::cout;

namespace
{
    struct Base
    {
        virtual char kind() { return 'B'; }
    };

    struct Base2
    {
        virtual char kind2() { return '2'; }
    };

    struct Derived : public Base, Base2
    {
        virtual char kind() { return 'D'; }
    };
}

constexpr bool compile_time_polymorphic_cast_check() {
#if defined(__cpp_constexpr) && __cpp_constexpr >= 201907L
    Derived derived;
    Base* base = &derived;
    return polymorphic_cast<Derived*>(base) != nullptr;
#endif
    return true;
}

static_assert(
    compile_time_polymorphic_cast_check(),
    "polymorphic_cast does not work at compile time"
);

constexpr bool compile_time_polymorphic_downcast_check() {
#if defined(__cpp_constexpr) && __cpp_constexpr >= 201907L
    Derived derived;
    Base* base = &derived;
    return polymorphic_downcast<Derived*>(base) != nullptr;
#endif
    return true;
}

static_assert(
    compile_time_polymorphic_downcast_check(),
    "polymorphic_downcast does not work at compile time"
);

constexpr bool compile_time_polymorphic_downcast2_check() {
#if defined(__cpp_constexpr) && __cpp_constexpr >= 201907L
    Derived derived;
    Base& base = derived;
    Derived& derived_again = polymorphic_downcast<Derived&>(base);
    (void)derived_again;
#endif
    return true;
}

static_assert(
    compile_time_polymorphic_downcast2_check(),
    "polymorphic_downcast does not work at compile time"
);

int main( int argc, char * argv[] )
{
#   ifdef NDEBUG
        cout << "NDEBUG is defined\n";
#   else
        cout << "NDEBUG is not defined\n";
#   endif

    cout << "\nBeginning tests...\n";

//  test polymorphic_cast  ---------------------------------------------------//

    //  tests which should succeed
    Derived derived_instance;
    Base * base = &derived_instance;
    Derived * derived = polymorphic_downcast<Derived*>( base );  // downcast
    BOOST_TEST( derived->kind() == 'D' );

    derived = polymorphic_cast<Derived*>( base ); // downcast, throw on error
    BOOST_TEST( derived->kind() == 'D' );

    Base2 *   base2 =  polymorphic_cast<Base2*>( base ); // crosscast
    BOOST_TEST( base2->kind2() == '2' );

     //  tests which should result in errors being detected
    Base base_instance;
    base = &base_instance;

    if ( argc > 1 && *argv[1] == '1' )
        { derived = polymorphic_downcast<Derived*>( base ); } // #1 assert failure

    bool caught_exception = false;
    try { derived = polymorphic_cast<Derived*>( base ); }
    catch (const std::bad_cast&)
        { cout<<"caught bad_cast\n"; caught_exception = true; }
    BOOST_TEST( caught_exception );
    //  the following is just so generated code can be inspected
    BOOST_TEST( derived->kind() != 'B' );

    return boost::report_errors();
}   // main

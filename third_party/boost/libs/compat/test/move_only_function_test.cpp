#include <boost/config.hpp>
#include <boost/config/workaround.hpp>

#if BOOST_WORKAROUND(BOOST_GCC, >= 7 * 10000 && BOOST_GCC < 8 * 10000)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wnoexcept-type"
#endif

#include <boost/compat/move_only_function.hpp>

#include <boost/compat/invoke.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>

#include <functional>
#include <memory>
#include <type_traits>

#if BOOST_WORKAROUND(BOOST_GCC, >= 13 * 10000)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wself-move"
#elif defined(BOOST_CLANG)
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wself-move"
#endif

using std::is_same;
using std::is_constructible;
using std::is_nothrow_constructible;

using boost::compat::move_only_function;
using boost::compat::in_place_type_t;
// using std::move_only_function;
// using std::in_place_type_t;
using boost::compat::invoke_result_t;
using boost::compat::is_invocable;
using boost::compat::is_nothrow_invocable;

template<class T>
using add_const_t = typename std::add_const<T>::type;

template<class T>
constexpr add_const_t<T>& as_const_ref( T& t ) noexcept
{
    return t;
}

void func0() {}

int func1()
{
    return 1;
}

int func2( int x )
{
    return x + 1;
}

int func3( std::unique_ptr<int> x )
{
    return *x + 1;
}

template<class T, class ...Args>
std::unique_ptr<T> make_unique( Args&&... args )
{
    return std::unique_ptr<T>( new T( std::forward<Args>( args )... ) );
}

struct callable
{
    std::unique_ptr<int> p_ = make_unique<int>( 1 );

    callable() = default;

    callable( std::unique_ptr<int> p ): p_( std::move( p ) )
    {
    }

    callable( std::initializer_list<int> il, int x, int y, int z )
    {
        int sum = 0;
        for( auto const v : il )
        {
            sum += v;
        }
        sum += x;
        sum += y;
        sum += z;
        p_ = make_unique<int>( sum );
    }

    callable( callable&& rhs ) noexcept
    {
        p_ = make_unique<int>( *rhs.p_ );
    }

    int operator()( int x )
    {
        return *p_ + x;
    }
};

struct noex_callable
{
    std::unique_ptr<int> p_ = make_unique<int>( 1 );

    noex_callable() = default;
    noex_callable( noex_callable&& rhs ) noexcept
    {
        p_ = make_unique<int>( *rhs.p_ );
    }

    noex_callable( noex_callable const& )
    {
    }

    int operator()( int x ) noexcept
    {
        return *p_ + x;
    }
};

struct large_callable
{
    unsigned char padding[ 256 ] = {};
    std::unique_ptr<int> p_ = make_unique<int>( 1 );

    large_callable() = default;

    large_callable( std::unique_ptr<int> p ): p_( std::move( p ) )
    {
    }

    large_callable( large_callable&& rhs ) noexcept
    {
        p_ = make_unique<int>( *rhs.p_ );
    }

    int operator()( int x )
    {
        return *p_ + x;
    }

    int operator()( int x, int y )
    {
        return x + y;
    }
};

struct person
{
    std::string name_;
    int age_ = -1;
    std::unique_ptr<int> p_ = make_unique<int>( 1 );

    int age()
    {
        return age_;
    }
};

static void test_call()
{
    {
        move_only_function<int()> f1;
        BOOST_TEST( !f1 );

        move_only_function<int() const> f2;
        BOOST_TEST( !f2 );
    }

    {
        move_only_function<int()> f1( nullptr );
        BOOST_TEST( !f1 );

        move_only_function<int() const> f2( nullptr );
        BOOST_TEST( !f2 );
    }

    {
        move_only_function<void()> f1( func0 );
        f1();
        BOOST_TEST( f1 );

        move_only_function<void() const> f2( func0 );
        f2();
        BOOST_TEST( f2 );
    }

    {
        int ( *fp )() = [] { return 0; };

        move_only_function<int()> f1( fp );
        BOOST_TEST( f1 );
        BOOST_TEST_EQ( f1(), 0 );
        BOOST_TEST_EQ( std::move( f1() ), 0 );

        move_only_function<int() const> f2( fp );
        BOOST_TEST( f2 );
        BOOST_TEST_EQ( f2(), 0 );
        BOOST_TEST_EQ( std::move( f2() ), 0 );

        fp = nullptr;
        move_only_function<int() const> f3( fp );
        BOOST_TEST_NOT( f3 );
    }

    {
        struct declared;
        enum class integer : int;

        move_only_function<void(declared)> f1;
        move_only_function<void(declared&)> f2;
        move_only_function<void(declared&&)> f3;
        move_only_function<void(integer)> f4;
    }

    {
        move_only_function<int()> f1( func1 );
        BOOST_TEST( f1 );
        BOOST_TEST_EQ( f1(), 1 );
        BOOST_TEST_EQ( std::move( f1() ), 1 );

        move_only_function<int() const> f2( func1 );
        BOOST_TEST( f2 );
        BOOST_TEST_EQ( f2(), 1 );
        BOOST_TEST_EQ( std::move( f2() ), 1 );
    }

    {
        move_only_function<int( std::unique_ptr<int> )> fn( func3 );
        BOOST_TEST_EQ( fn( make_unique<int>( 1 ) ), 2 );
        BOOST_TEST_EQ( std::move( fn )( make_unique<int>( 1 ) ), 2 );

        auto x = make_unique<int>( 1 );
        BOOST_TEST( fn );
        BOOST_TEST_EQ( fn( std::move( x ) ), 2 );
    }

    {
        auto l = []() { return 1; };

        move_only_function<int()> f1( l );
        BOOST_TEST( f1 );
        BOOST_TEST_EQ( f1(), 1 );
        BOOST_TEST_EQ( std::move( f1 )(), 1 );

        move_only_function<int() const> f2( l );
        BOOST_TEST( f2 );
        BOOST_TEST_EQ( f2(), 1 );
        BOOST_TEST_EQ( as_const_ref( f2 )(), 1 );
        BOOST_TEST_EQ( std::move( f2 )(), 1 );
        BOOST_TEST_EQ( std::move( as_const_ref( f2 ) )(), 1 );
    }

    {
        move_only_function<int()> f1( []() { return 1; } );
        BOOST_TEST( f1 );
        BOOST_TEST_EQ( f1(), 1 );
        BOOST_TEST_EQ( std::move( f1 )(), 1 );

        move_only_function<int() const> f2( []() { return 1; } );
        BOOST_TEST( f2 );
        BOOST_TEST_EQ( f2(), 1 );
        BOOST_TEST_EQ( as_const_ref( f2 )(), 1 );
        BOOST_TEST_EQ( std::move( f2 )(), 1 );
        BOOST_TEST_EQ( std::move( as_const_ref( f2 ) )(), 1 );
    }

    {
        auto l = []( int x ) { return x + 1; };

        move_only_function<int( int )> f1( l );
        BOOST_TEST( f1 );
        BOOST_TEST_EQ( f1( 1 ), 2 );

        move_only_function<int( int ) const> f2( l );
        BOOST_TEST( f2 );
        BOOST_TEST_EQ( f2( 1 ), 2 );
    }

    {
        auto l = []( std::unique_ptr<int> x ) { return *x + 1; };
        move_only_function<int( std::unique_ptr<int> )> f( std::move( l ) );
        BOOST_TEST( f );
        BOOST_TEST_EQ( f( make_unique<int>( 1 ) ), 2 );
    }

    {
        auto p = make_unique<int>( 1 );
        auto l = []( std::unique_ptr<int>& x ) { return *x + 1; };
        move_only_function<int( std::unique_ptr<int>& )> f( l );
        BOOST_TEST( f );
        BOOST_TEST_EQ( f( p ), 2 );
    }

    {
        int x = 1;
        move_only_function<int( int )> f1( callable{} );
        BOOST_TEST( f1 );
        BOOST_TEST_EQ( f1( 1 ), 2);
        BOOST_TEST_EQ( f1( x ), 2 );

        int y = 2;
        callable c;
        move_only_function<int( int )> f2( std::move( c ) );
        BOOST_TEST( f2 );
        BOOST_TEST_EQ( f2( 2 ), 3 );
        BOOST_TEST_EQ( f2( y ), 3 );
    }

    {
        move_only_function<int( int, int )> f1( large_callable{} );
        BOOST_TEST( f1 );
        BOOST_TEST_EQ( f1( 1, 2 ), 3);

        large_callable c;
        move_only_function<int( int, int )> f2( std::move( c ) );
        BOOST_TEST( f2 );
        BOOST_TEST_EQ( f2( 1, 2 ), 3 );
    }

    {
        move_only_function<int( int )> f1( in_place_type_t<callable>{}, make_unique<int>( 4321 ) );
        BOOST_TEST_EQ( f1( 1234 ), 5555 );

        move_only_function<int( int )> f2( in_place_type_t<large_callable>{}, make_unique<int>( 4321 ) );
        BOOST_TEST_EQ( f2( 1234 ), 5555 );

        move_only_function<int( int )> f3( in_place_type_t<int(*)( int )>{}, func2 );
        BOOST_TEST_EQ( f3( 1233 ), 1234 );

        move_only_function<int( int )> f4( in_place_type_t<int(*)( int )>{} );
        BOOST_TEST( f4 != nullptr );

        move_only_function<int( int )> f5( in_place_type_t<callable>{}, std::initializer_list<int>{ 1, 2, 3 }, 4, 5, 6 );
        BOOST_TEST_EQ( f5( 7 ), 1 + 2 + 3 + 4 + 5 + 6 + 7 );
    }

    {
        move_only_function<int()> f1( func1 );
        move_only_function<int()> f2( std::move( f1 ) );

        BOOST_TEST( !f1 );
        BOOST_TEST( f2 );
        BOOST_TEST_EQ( f2(), 1 );
    }

    {
        move_only_function<int( int )> f1( callable{} );
        move_only_function<int( int )> f2( std::move( f1 ) );

        BOOST_TEST( !f1 );
        BOOST_TEST( f2 );
        BOOST_TEST_EQ( f2( 2 ), 3 );
    }

    {
        move_only_function<int( int, int )> f1( large_callable{} );
        move_only_function<int( int, int )> f2( std::move( f1 ) );

        BOOST_TEST( !f1 );
        BOOST_TEST( f2 );
        BOOST_TEST_EQ( f2( 1, 2 ), 3 );
    }

    {
        person p;
        p.age_ = 35;

        move_only_function<int( person& )> f1( &person::age_ );
        BOOST_TEST_EQ( f1( p ), 35 );

        p.age_ = 53;

        move_only_function<int( person& )> f2( &person::age );
        BOOST_TEST_EQ( f2( p ), 53 );

        int person::*mp = nullptr;
        move_only_function<int( person& )> f3( mp );
        BOOST_TEST_NOT( f3 );

        int (person::*mfp)() = nullptr;
        move_only_function<int( person& )> f4( mfp );
        BOOST_TEST_NOT( f4 );
    }

    {
        struct tester
        {
            int operator()()
            {
                return 1;
            }

            int operator()() const
            {
                return 2;
            }
        };

        tester t;

        move_only_function<int()> f1( t );
        move_only_function<int() const> f2( t );

        BOOST_TEST_EQ( f1(), 1 );
        BOOST_TEST_EQ( std::move( f1 )(), 1 );
        BOOST_TEST_EQ( f2(), 2 );
        BOOST_TEST_EQ( as_const_ref( f2 )(), 2 );
        BOOST_TEST_EQ( std::move( as_const_ref( f2 ) )(), 2 );
    }

    {
        struct tester
        {
            int operator()() noexcept
            {
                return 1;
            }

            int operator()() const noexcept
            {
                return 2;
            }
        };

        tester t;

        move_only_function<int() noexcept> f1( t );
        move_only_function<int() const noexcept> f2( t );

        BOOST_TEST_EQ( f1(), 1 );
        BOOST_TEST_EQ( std::move( f1 )(), 1 );
        BOOST_TEST_EQ( f2(), 2 );
        BOOST_TEST_EQ( as_const_ref( f2 )(), 2 );
        BOOST_TEST_EQ( std::move( as_const_ref( f2 ) )(), 2 );
    }

    {
        struct tester
        {
            int operator()() &
            {
                return 1;
            }

            int operator()() &&
            {
                return 2;
            }

            int operator()() const&
            {
                return 3;
            }

            int operator()() const&&
            {
                return 4;
            }
        };

        tester t;

        move_only_function<int() &> f1( t );
        move_only_function<int() &&> f2( t );
        move_only_function<int() const&> f3( t );

        BOOST_TEST_EQ( f1(), 1 );
        BOOST_TEST_EQ( std::move( f2 )(), 2 );
        BOOST_TEST_EQ( f3(), 3 );
        BOOST_TEST_EQ( as_const_ref( f3 )(), 3 );

#if !BOOST_WORKAROUND(BOOST_GCC, < 40900)

        move_only_function<int() const&&> f4( t );
        BOOST_TEST_EQ( std::move( f4 )(), 4 );
        BOOST_TEST_EQ( std::move( as_const_ref( f4 ) )(), 4 );

#endif
    }

    {
        struct tester
        {
            int operator()() & noexcept
            {
                return 1;
            }

            int operator()() && noexcept
            {
                return 2;
            }

            int operator()() const& noexcept
            {
                return 3;
            }

            int operator()() const&& noexcept
            {
                return 4;
            }
        };

        tester t;

        move_only_function<int() &> f1( t );
        move_only_function<int() &&> f2( t );
        move_only_function<int() const&> f3( t );

        BOOST_TEST_EQ( f1(), 1 );
        BOOST_TEST_EQ( std::move( f2 )(), 2 );
        BOOST_TEST_EQ( f3(), 3 );
        BOOST_TEST_EQ( as_const_ref( f3 )(), 3 );

#if !BOOST_WORKAROUND(BOOST_GCC, < 40900)

        move_only_function<int() const&&> f4( t );
        BOOST_TEST_EQ( std::move( f4 )(), 4 );
        BOOST_TEST_EQ( std::move( as_const_ref( f4 ) )(), 4 );

#endif
    }

    {
        move_only_function<int( int )> f1( callable{} );
        // f1 = std::move( f1 );
        // BOOST_TEST_EQ( f1( 1233 ), 1234 );

        move_only_function<int( int )> f2( large_callable{} );
        f2 = std::move( f1 );
        BOOST_TEST_EQ( f2( 1233 ), 1234 );
        BOOST_TEST_NOT( f1 );

        move_only_function<int( int )> f3( callable{} );
        move_only_function<int( int )> f4( large_callable{} );
        f3 = std::move( f4 );
        BOOST_TEST_EQ( f3( 1233 ), 1234 );
        BOOST_TEST_NOT( f4 );

        move_only_function<int( int )> f5( callable{} );
        f5 = nullptr;
        BOOST_TEST_NOT( f5 );

        move_only_function<int( int )> f6( large_callable{} );
        f6 = nullptr;
        BOOST_TEST_NOT( f6 );

        move_only_function<int( int )> f7;
        f7 = nullptr;
        BOOST_TEST_NOT( f7 );

        move_only_function<int( int )> f8( callable{} );
        f8 = large_callable{};
        BOOST_TEST_EQ( f8( 1233 ), 1234 );

        move_only_function<int( int )> f9( large_callable{} );
        f9 = callable{};
        BOOST_TEST_EQ( f9( 1233 ), 1234 );

        move_only_function<int( int )> f10;
        f10 = callable{};
        BOOST_TEST_EQ( f10( 1233 ), 1234 );

        move_only_function<int( int )> f11;
        f11 = large_callable{};
        BOOST_TEST_EQ( f11( 1233 ), 1234 );
    }

    {
        callable c1;
        *c1.p_ = 1234;

        large_callable c2;
        *c2.p_ = 4321;

        move_only_function<int( int )> f1( std::move( c1 ) );
        move_only_function<int( int )> f2( std::move( c2 ) );

        int x = 1;

        BOOST_TEST_EQ( f1( x ), 1235 );
        BOOST_TEST_EQ( f2( x ), 4322 );

        swap( f1, f2 );

        BOOST_TEST_EQ( f1( x ), 4322 );
        BOOST_TEST_EQ( f2( x ), 1235 );

        move_only_function<int( int )> f3( callable{} );
        move_only_function<int( int )> f4;

        swap( f3, f4 );
        BOOST_TEST_NOT( f3 );
        BOOST_TEST( f4 );

        move_only_function<int( int )> f5;
        move_only_function<int( int )> f6;

        swap( f5, f6 );
        BOOST_TEST_NOT( f5 );
        BOOST_TEST_NOT( f6 );
    }

    {
        struct throwing
        {
            static int x()
            {
                throw 1234;
            }

            int operator()()
            {
                throw 1234;
            }
        };

        struct large_throwing
        {
            char padding[ 256 ] = {};

            int operator()()
            {
                throw 1234;
            }
        };

        move_only_function<int()> f1( &throwing::x );
        move_only_function<int()> f2( throwing{} );
        move_only_function<int()> f3( large_throwing{} );

        BOOST_TEST_THROWS( f1(), int );
        BOOST_TEST_THROWS( f2(), int );
        BOOST_TEST_THROWS( f3(), int );
    }
}

struct Q
{
    void operator()() const &;
    void operator()() &&;
};

struct R
{
    void operator()() &;
    void operator()() &&;
};

// These types are all small and nothrow move constructible
struct F { void operator()(); };
struct G { void operator()() const; };

struct H
{
    H( int );
    H( int, int ) noexcept;
    void operator()() noexcept;
};

struct I
{
    I( int, const char* );
    I( std::initializer_list<char> );
    int operator()() const noexcept;
};

static void test_traits()
{
    // just copy the static assertions from libstdc++'s test suite, call.cc, cons.cc, conv.cc

    // Check return types
    BOOST_TEST_TRAIT_TRUE( ( is_same<void, invoke_result_t<move_only_function<void()>>> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_same<int, invoke_result_t<move_only_function<int()>>> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_same<int&, invoke_result_t<move_only_function<int&()>>> ) );

    // With const qualifier
    BOOST_TEST_TRAIT_FALSE( ( is_invocable< move_only_function<void()> const > ) );
    BOOST_TEST_TRAIT_FALSE( ( is_invocable< move_only_function<void()> const &> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_invocable< move_only_function<void() const> > ) );
    BOOST_TEST_TRAIT_TRUE( ( is_invocable< move_only_function<void() const> &> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_invocable< move_only_function<void() const> const > ) );
    BOOST_TEST_TRAIT_TRUE( ( is_invocable< move_only_function<void() const> const &> ) );

    // With no ref-qualifier
    BOOST_TEST_TRAIT_TRUE( ( is_invocable< move_only_function<void()> > ) );
    BOOST_TEST_TRAIT_TRUE( ( is_invocable< move_only_function<void()> &> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_invocable< move_only_function<void() const> > ) );
    BOOST_TEST_TRAIT_TRUE( ( is_invocable< move_only_function<void() const> &> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_invocable< move_only_function<void() const> const > ) );
    BOOST_TEST_TRAIT_TRUE( ( is_invocable< move_only_function<void() const> const &> ) );

    // With & ref-qualifier
    BOOST_TEST_TRAIT_FALSE( ( is_invocable< move_only_function<void()&> > ) );
    BOOST_TEST_TRAIT_TRUE( ( is_invocable< move_only_function<void()&> &> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_invocable< move_only_function<void() const&> > ) );
    BOOST_TEST_TRAIT_TRUE( ( is_invocable< move_only_function<void() const&> &> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_invocable< move_only_function<void() const&> const > ) );
    BOOST_TEST_TRAIT_TRUE( ( is_invocable< move_only_function<void() const&> const &> ) );

    // With && ref-qualifier
    BOOST_TEST_TRAIT_TRUE( ( is_invocable< move_only_function<void()&&> > ) );
    BOOST_TEST_TRAIT_FALSE( ( is_invocable< move_only_function<void()&&> &> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_invocable< move_only_function<void() const&&> > ) );
    BOOST_TEST_TRAIT_FALSE( ( is_invocable< move_only_function<void() const&&> &> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_invocable< move_only_function<void() const&&> const > ) );
    BOOST_TEST_TRAIT_FALSE( ( is_invocable< move_only_function<void() const&&> const &> ) );

    #if defined(__cpp_noexcept_function_type)

    // With noexcept-specifier
    BOOST_TEST_TRAIT_FALSE( ( is_nothrow_invocable< move_only_function<void()> > ) );
    BOOST_TEST_TRAIT_FALSE( ( is_nothrow_invocable< move_only_function<void() noexcept(false)> > ) );
    BOOST_TEST_TRAIT_TRUE( ( is_nothrow_invocable< move_only_function<void() noexcept> > ) );
    BOOST_TEST_TRAIT_TRUE( ( is_nothrow_invocable< move_only_function<void()& noexcept>& > ) );

    #endif

    BOOST_TEST_TRAIT_TRUE( ( std::is_nothrow_default_constructible<move_only_function<void()>> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_nothrow_constructible<move_only_function<void()>, std::nullptr_t> ) );
    BOOST_TEST_TRAIT_TRUE( ( std::is_nothrow_move_constructible<move_only_function<void()>> ) );
    BOOST_TEST_TRAIT_FALSE( ( std::is_copy_constructible<move_only_function<void()>> ) );

    BOOST_TEST_TRAIT_TRUE( ( is_constructible<move_only_function<void()>, void()> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_constructible<move_only_function<void()>, void( & )()> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_constructible<move_only_function<void()>, void( * )()> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_constructible<move_only_function<void()>, int()> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_constructible<move_only_function<void()>, int( & )()> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_constructible<move_only_function<void()>, int( * )()> ) );
    BOOST_TEST_TRAIT_FALSE( ( is_constructible<move_only_function<void()>, void( int )> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_constructible<move_only_function<void(int)>, void( int )> ) );

    BOOST_TEST_TRAIT_TRUE( ( is_constructible<move_only_function<void( int )>, in_place_type_t<void(*)( int )>, void( int )> ) );

    BOOST_TEST_TRAIT_TRUE( ( is_constructible<move_only_function<void()>, void() noexcept> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_constructible<move_only_function<void() noexcept>, void() noexcept> ) );

#if defined(__cpp_noexcept_function_type)
    BOOST_TEST_TRAIT_FALSE( ( is_constructible<move_only_function<void() noexcept>, void()> ) );
#endif

    BOOST_TEST_TRAIT_TRUE( ( is_constructible<move_only_function<void()>, Q> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_constructible<move_only_function<void() const>, Q> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_constructible<move_only_function<void() &>, Q> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_constructible<move_only_function<void() const &>, Q> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_constructible<move_only_function<void() &&>, Q> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_constructible<move_only_function<void() const &&>, Q> ) );

    BOOST_TEST_TRAIT_TRUE( ( is_constructible<move_only_function<void()>, R> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_constructible<move_only_function<void()&>, R> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_constructible<move_only_function<void()&&>, R> ) );
    BOOST_TEST_TRAIT_FALSE( ( is_constructible<move_only_function<void() const>, R> ) );
    BOOST_TEST_TRAIT_FALSE( ( is_constructible<move_only_function<void() const&>, R> ) );
    BOOST_TEST_TRAIT_FALSE( ( is_constructible<move_only_function<void() const&&>, R> ) );

    BOOST_TEST_TRAIT_TRUE( ( is_nothrow_constructible<move_only_function<void()>, F> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_nothrow_constructible<move_only_function<void()>, G> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_nothrow_constructible<move_only_function<void() const>, G> ) );

    BOOST_TEST_TRAIT_TRUE( ( is_nothrow_constructible<move_only_function<void()>, H> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_nothrow_constructible<move_only_function<void() noexcept>, H> ) );
    BOOST_TEST_TRAIT_FALSE( ( is_nothrow_constructible<move_only_function<void() noexcept >, in_place_type_t<H>, int> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_nothrow_constructible<move_only_function<void() noexcept>, in_place_type_t<H>, int, int> ) );

    BOOST_TEST_TRAIT_TRUE( ( is_constructible<move_only_function<void()>, in_place_type_t<I>, int, const char*> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_constructible<move_only_function<void()>, in_place_type_t<I>, std::initializer_list<char>> ) );

    BOOST_TEST_TRAIT_FALSE( ( is_constructible<move_only_function<void()>, move_only_function<void() &>> ) );
    BOOST_TEST_TRAIT_FALSE( ( is_constructible<move_only_function<void()>, move_only_function<void() &&>> ) );
    BOOST_TEST_TRAIT_FALSE( ( is_constructible<move_only_function<void()&>, move_only_function<void() &&>> ) );
    BOOST_TEST_TRAIT_FALSE( ( is_constructible<move_only_function<void() const>, move_only_function<void()>> ) );

    using FuncType = int( int );

    // Top level const qualifiers are ignored in function types, and decay
    // is performed.
    BOOST_TEST_TRAIT_TRUE( ( is_same<move_only_function<void( int const )>, move_only_function<void( int )>> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_same<move_only_function<void( int[ 2 ] )>, move_only_function<void( int* )>> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_same<move_only_function<void( int[] )>, move_only_function<void( int* )>> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_same<move_only_function<void(int const[ 5 ])>, move_only_function<void( int const* )>> ) );
    BOOST_TEST_TRAIT_TRUE( ( is_same<move_only_function<void( FuncType )>, move_only_function<void( FuncType* )>> ) );
}

static void test_conv()
{
    {
        noex_callable nt;

        move_only_function<int( int ) noexcept> f1( std::move( nt ) );
        move_only_function<int( int )> f2( std::move( f1 ) );
        BOOST_TEST_EQ( f2( 1234 ), 1235 );
    }

    {
        auto l = []( noex_callable const& c ) noexcept { return *c.p_ + 1234; };

        noex_callable c;

        move_only_function<int( noex_callable ) const noexcept> f1( l );
        BOOST_TEST_EQ( f1( c ), 1235 );

        move_only_function<int( noex_callable ) const> f2( std::move( f1 ) );
        BOOST_TEST_EQ( f2( c ), 1235 );

        move_only_function<int( noex_callable )> f3( std::move( f2 ) );
        BOOST_TEST_EQ( f3( c ), 1235 );

        move_only_function<int( noex_callable&& )> f4( std::move( f3 ) );
        BOOST_TEST_EQ( f4( noex_callable{} ), 1235 );

        move_only_function<int( noex_callable&& ) &&> f5( std::move( f4 ) );
        BOOST_TEST_EQ( std::move( f5 )( noex_callable{} ), 1235 );

        move_only_function<int( noex_callable&& )> f6( l );

        move_only_function<int( noex_callable&& ) &> f7( std::move( f6 ) );
        BOOST_TEST_EQ( f7( noex_callable{} ), 1235 );

        // TODO: libstdc++ includes this test case but it seems like pedantically calling a `long(*)(Arg)` should
        // be UB in the general sense. We need to confirm one way or another if this is something we need to support.
        //
        // move_only_function<int( noex_callable ) const noexcept> f8( l );
        // move_only_function<long( noex_callable ) const noexcept> f9( std::move( f8 ) );
        // BOOST_TEST_EQ( f9( noex_callable{} ), 1235 );
    }

    {
        move_only_function<int( long ) const noexcept> e;
        BOOST_TEST( e == nullptr );

        move_only_function<int( long ) const> e2( std::move( e ) );
        BOOST_TEST( e2 == nullptr );
        e2 = std::move( e );
        BOOST_TEST( e2 == nullptr );

        move_only_function<int( long )> e3( std::move( e2 ) );
        BOOST_TEST( e3 == nullptr );
    }
}

int main()
{
    test_call();
    test_traits();
    test_conv();

    return boost::report_errors();
}

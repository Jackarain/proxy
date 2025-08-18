//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_EXTRA_TEST_SUITE_HPP
#define BOOST_URL_EXTRA_TEST_SUITE_HPP

#if defined(_MSC_VER)
# pragma once
#endif

#include <boost/current_function.hpp>
#include <cctype>
#include <sstream>
#include <type_traits>

//  This is a derivative work
//  Copyright 2002-2018 Peter Dimov
//  Copyright (c) 2002, 2009, 2014 Peter Dimov
//  Copyright (2) Beman Dawes 2010, 2011
//  Copyright (3) Ion Gaztanaga 2013
//
//  Copyright 2018 Glen Joseph Fernandes
//  (glenjofe@gmail.com)

namespace test_suite {

//------------------------------------------------

/** Represents a type-erased test suite for unit testing.

    This abstract base class defines the interface for a test suite,
    allowing derived classes to implement specific test logic.

    It provides methods to retrieve the name of the test suite
    and to execute the test suite.

    @note This class is intended to be used as a base class for
          concrete test suite implementations.
 */
struct any_suite
{
    /** Virtual destructor for the `any_suite` class.

        Ensures proper cleanup of derived classes.
     */
    virtual ~any_suite() = 0;

    /** Retrieves the name of the test suite.

        A string literal representing the name of the test suite.
     */
    virtual char const* name() const noexcept = 0;

    /** Executes the test suite.

        This method should be overridden by derived classes to
        implement the specific test logic.
     */
    virtual void run() const = 0;
};
//------------------------------------------------

/** Represents a collection of type erased test suites for unit testing.

    This abstract class provides an interface for managing a collection
    of test suites.

    It allows the insertion of test suites and provides
    iterators to traverse the collection.

    @note The `sort()` method is deprecated and should not be used in new code.
 */
struct suites
{
    virtual ~suites() = default;

    using iterator = any_suite const* const*;

    /** Inserts a test suite into the collection.

        @param suite The test suite to insert.
     */
    virtual void insert(any_suite const&) = 0;

    /** Returns an iterator to the beginning of the collection.

        @return An iterator pointing to the first test suite in the collection.
     */
    virtual iterator begin() const noexcept = 0;

    /** Returns an iterator to the end of the collection.

        @return An iterator pointing past the last test suite in the collection.
     */
    virtual iterator end() const noexcept = 0;

    /** Sorts the test suites in the collection.

        This method is deprecated and should not be used.
     */
    virtual void sort() = 0;

    /** Provides access to the global instance of the `suites` collection.

        A reference to the global `suites` instance.
     */
    static suites& instance() noexcept;
};
//------------------------------------------------

/** Represents a test suite for unit testing.

    This class template defines a test suite `T` that can be registered
    with the global list of test suites managed by `test_suite::suites`
    via the `TEST_SUITE` macro.

    It inherits from `any_suite` and provides the implementation for
    the `name` and `run` methods.

    @tparam T The class type that implements the test suite. This class
              must have a `run()` method that contains the test logic.

    @note The `T` class must be default-constructible.
 */
template<class T>
class suite : public any_suite
{
    char const* name_; ///< The name of the test suite.

public:
    /** Constructs a `suite` object and registers it.

        @param name A string literal representing the name of the test suite.
     */
    explicit
    suite(char const* name) noexcept
        : name_(name)
    {
        suites::instance().insert(*this);
    }

    /** Returns the name of the test suite.

        @return A string literal representing the name of the test suite.
     */
    char const*
    name() const noexcept override
    {
        return name_;
    }

    /** Executes the test suite.

        This method creates an instance of the `T` class and calls
        its `run()` method.
     */
    void
    run() const override
    {
        T().run();
    }
};

//------------------------------------------------

/** Manages the execution and logging of test suites.

    This abstract base class provides an interface for running test suites,
    logging messages, and evaluating test conditions.

    It maintains a stack of `any_runner` instances to allow nested
    test runners.

    @note This class is intended to be extended by concrete implementations
          that define specific behavior for running tests and logging.
 */
class any_runner
{
    // Pointer to the previous `any_runner` instance in the stack.
    any_runner* prev_;

    /** Retrieves the pointer to the current `any_runner` instance.

        @return A reference to the pointer of the current `any_runner` instance.
     */
    static any_runner*& instance_impl() noexcept;

public:
    /** Retrieves the current `any_runner` instance.

        @return A reference to the current `any_runner` instance.
     */
    static any_runner& instance() noexcept;

    /** Constructs an `any_runner` and pushes it onto the stack.
     */
    any_runner() noexcept;

    /** Destroys the `any_runner` and removes it from the stack.
     */
    virtual ~any_runner();

    /** Executes a test suite.

        @param test The test suite to execute.
     */
    virtual void run(any_suite const& test) = 0;

    /** Logs a message.

       @param msg The message to log.
     */
    virtual void note(char const* msg) = 0;

    /** Evaluates a test condition and logs the result.

       @param cond The condition to evaluate.
       @param expr The string representation of the condition.
       @param func The name of the function where the test is evaluated.
       @param file The name of the file where the test is evaluated.
       @param line The line number where the test is evaluated.
       @return `true` if the condition is true, otherwise `false`.
     */
    virtual bool test(bool cond,
        char const* expr, char const* func,
        char const* file, int line) = 0;

    /** Provides access to the log stream.

       @return A reference to the log stream.
     */
    virtual std::ostream& log() noexcept = 0;
};

//------------------------------------------------

#ifndef _MSC_VER

/** Alias for a debug stream.

    This alias is used to define a `debug_stream` as a reference to a standard
    output stream (`std::ostream&`). On non-Microsoft platforms, it simplifies
    the implementation by directly using the standard output stream without
    additional functionality.

    @note On Microsoft platforms, `debug_stream` is implemented as a custom
          class to redirect output to the Visual Studio debugger.
 */
using debug_stream = std::ostream&;

#else

namespace detail {

class debug_streambuf
    : public std::stringbuf
{
    std::ostream& os_;
    bool dbg_;

    void
    write(char const* s);

public:
    explicit
    debug_streambuf(
        std::ostream& os);

    ~debug_streambuf();

    int sync() override;
};

} // detail

//------------------------------------------------

/** std::ostream with Visual Studio IDE redirection.

    Instances of this stream wrap a specified
    `ostream` (such as `cout` or `cerr`). If a
    debugger is attached when the stream is
    created, output will additionally copied
    to the debugger's output window.
*/
class debug_stream : public std::ostream
{
    detail::debug_streambuf buf_;

public:
    /** Construct a stream.

        @param os The output stream to wrap.
    */
    explicit
    debug_stream(
        std::ostream& os)
        : std::ostream(&buf_)
        , buf_(os)
    {
        if(os.flags() & std::ios::unitbuf)
            std::unitbuf(*this);
    }
};

#endif


//------------------------------------------------

/** Provide the command-line arguments to the test suite.

    This function forwards the command-line arguments passed to the application
    to the test suite. It allows the test suite to access command-line arguments
    for filtering test suites or for other purposes.

    @param argc The number of command-line arguments.
    @param argv The array of command-line arguments.
    @return `EXIT_SUCCESS` if the application was able to process the
            command line successfully, otherwise `EXIT_FAILURE`.
 */
void
set_command_line(
    int argc,
    char const* const* argv);

/** Returns the number of command-line arguments.

    This function returns the number of command-line arguments
    that were passed to the application.

    This information must be set using the `set_command_line()` function
    before calling this function, otherwise it will throw an exception.

 */
int
argc();

/** Returns the command-line arguments.

    This function returns a pointer to an array of C-style strings
    representing the command-line arguments passed to the application.

    This information must be set using the `set_command_line()` function
    before calling this function, otherwise it will throw an exception.

 */
char const* const*
argv();

/** Return a command-line option

    This function returns the value of a command-line option
    if it exists, otherwise it returns `nullptr`.

    A command-line option can be set with the `--name=value`
    format or the `--name value` format. A short name can
    also be specified with a single dash, like `-n=value`
    or `-n value`.

    @param short_name The name of the command-line option to search for.
    @param name The name of the command-line option to search for.
    @return The value of the command-line option or an empty string if not found.
 */
char const*
get_command_line_option(
    char const* short_name,
    char const* name);

/** Return a command-line option
 */
inline
char const*
get_command_line_option(char const* name)
{
    return get_command_line_option(nullptr, name);
}

/** Return a command-line option
 */
inline
char const*
get_command_line_short_option(char const* short_name)
{
    return get_command_line_option(short_name, nullptr);
}

/** Return a command-line flag

    This function returns the value of a command-line flag
    if it exists, otherwise it returns `false`.

    A command-line flag can be set with the `--name` format
    or the `-n` format.

    @param name The name of the command-line flag to search for.
    @return The value of the command-line flag or an empty string if not found.
 */
bool
get_command_line_flag(
    char const* short_name,
    char const* name);

/** Return a command-line flag
 */
inline
bool
get_command_line_flag(char const* name)
{
    return get_command_line_flag(nullptr, name);
}

/** Return a command-line flag
 */
inline
bool
get_command_line_short_flag(char const* short_name)
{
    return get_command_line_flag(short_name, nullptr);
}

/** Runs the test suites and logs the results.

    This function executes the registered test suites, optionally filtered
    by command-line arguments, and logs the results to the provided output stream.

    - If no arguments are provided, all test suites are executed.
    - If arguments are provided, only test suites whose names match the arguments are executed.
    - The function also supports a `-h` or `--help` argument to display usage information.

    @param out The output stream to log the results.
    @param argc The number of command-line arguments.
    @param argv The array of command-line arguments.
    @return `EXIT_SUCCESS` if all tests pass, otherwise `EXIT_FAILURE`.
 */
int
run(std::ostream& out,
    int argc,
    char const* const* argv);

int
run(std::ostream& out);

int
run(int argc,
    char const* const* argv);

int
run();

//------------------------------------------------

namespace detail {

// In the comparisons below, it is possible that
// T and U are signed and unsigned integer types,
// which generates warnings in some compilers.
// A cleaner fix would require common_type trait
// or some meta-programming, which would introduce
// a dependency on Boost.TypeTraits. To avoid
// the dependency we just disable the warnings.
#if defined(__clang__) && defined(__has_warning)
# if __has_warning("-Wsign-compare")
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wsign-compare"
# endif
#elif defined(_MSC_VER)
# pragma warning(push)
# pragma warning(disable: 4389)
# pragma warning(disable: 4018)
#elif defined(__GNUC__) && !(defined(__INTEL_COMPILER) || defined(__ICL) || defined(__ICC) || defined(__ECC)) && (__GNUC__ * 100 + __GNUC_MINOR__) >= 406
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wsign-compare"
#endif

template<class...>
struct make_void
{
    using type = void;
};

template<class... Ts>
using void_t = typename make_void<Ts...>::type;

template <class T, class = void>
struct is_streamable : std::false_type
{};

template <class T>
struct is_streamable<
    T, void_t<decltype(std::declval<
        std::ostream&>() << std::declval<T&>())
    > > : std::true_type
{};

template <class T>
auto
test_output_impl(T const& v) ->
    typename std::enable_if<
        is_streamable<T>::value,
        T const&>::type
{
    return v;
}

template <class T>
auto
test_output_impl(T const&) ->
    typename std::enable_if<
        ! is_streamable<T>::value,
        std::string>::type
{
    return "?";
}

// specialize test output for char pointers to avoid printing as cstring
template<class T>
       const void* test_output_impl(T volatile* v) { return const_cast<T*>(v); }
inline const void* test_output_impl(const char* v) { return v; }
inline const void* test_output_impl(const unsigned char* v) { return v; }
inline const void* test_output_impl(const signed char* v) { return v; }
inline const void* test_output_impl(char* v) { return v; }
inline const void* test_output_impl(unsigned char* v) { return v; }
inline const void* test_output_impl(signed char* v) { return v; }
inline const void* test_output_impl(std::nullptr_t) { return nullptr; }

// print chars as numeric
inline int test_output_impl( signed char const& v ) { return v; }
inline unsigned test_output_impl( unsigned char const& v ) { return v; }

// Whether wchar_t is signed is implementation-defined
template<bool Signed> struct lwt_long_type {};
template<> struct lwt_long_type<true> { typedef long type; };
template<> struct lwt_long_type<false> { typedef unsigned long type; };
inline lwt_long_type<
    (static_cast<wchar_t>(-1) < static_cast<wchar_t>(0))
        >::type test_output_impl( wchar_t const& v ) { return v; }

#if !defined( BOOST_NO_CXX11_CHAR16_T )
inline unsigned long test_output_impl( char16_t const& v ) { return v; }
#endif

#if !defined( BOOST_NO_CXX11_CHAR32_T )
inline unsigned long test_output_impl( char32_t const& v ) { return v; }
#endif

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996)
#endif

inline std::string test_output_impl( char const& v )
{
    if( std::isprint( static_cast<unsigned char>( v ) ) )
    {
        return { 1, v };
    }
    else
    {
        char buffer[ 8 ];
        std::snprintf( buffer, sizeof(buffer), "\\x%02X", static_cast<unsigned char>( v ) );
        return buffer;
    }
}

bool
test_impl(
    bool cond,
    char const* expr,
    char const* func,
    char const* file,
    int line);

void
throw_failed_impl(
    const char* expr,
    char const* excep,
    char const* func,
    char const* file,
    int line);

void
no_throw_failed_impl(
    const char* expr,
    char const* excep,
    char const* func,
    char const* file,
    int line);

void
no_throw_failed_impl(
    const char* expr,
    char const* func,
    char const* file,
    int line);

struct lw_test_eq
{
    template <typename T, typename U>
    bool operator()(const T& t, const U& u) const
    {
        return t == u;
    }
};

struct lw_test_ne
{

    template <typename T, typename U>
    bool operator()(const T& t, const U& u) const
    {
        return t != u;
    }
};

struct lw_test_lt
{
    template <typename T, typename U>
    bool operator()(const T& t, const U& u) const
    {
        return t < u;
    }
};

struct lw_test_gt
{
    template <typename T, typename U>
    bool operator()(const T& t, const U& u) const
    {
        return t > u;
    }
};

struct lw_test_le
{
    template <typename T, typename U>
    bool operator()(const T& t, const U& u) const
    {
        return t <= u;
    }
};

struct lw_test_ge
{
    template <typename T, typename U>
    bool operator()(const T& t, const U& u) const
    {
        return t >= u;
    }
};

// lwt_predicate_name

template<class T> char const * lwt_predicate_name( T const& )
{
    return "~=";
}

inline char const * lwt_predicate_name( lw_test_eq const& )
{
    return "==";
}

inline char const * lwt_predicate_name( lw_test_ne const& )
{
    return "!=";
}

inline char const * lwt_predicate_name( lw_test_lt const& )
{
    return "<";
}

inline char const * lwt_predicate_name( lw_test_le const& )
{
    return "<=";
}

inline char const * lwt_predicate_name( lw_test_gt const& )
{
    return ">";
}

inline char const * lwt_predicate_name( lw_test_ge const& )
{
    return ">=";
}

//------------------------------------------------

template<class Pred, class T, class U>
bool
test_with_impl(
    Pred pred,
    char const* expr1,
    char const* expr2,
    char const* func,
    char const* file,
    int line,
    T const& t, U const& u)
{
    if(pred(t, u))
    {
        any_runner::instance().test(
            true, "", func, file, line);
        return true;
    }
    std::stringstream ss;
    ss <<
        "\"" << test_output_impl(t) << "\" " <<
        lwt_predicate_name(pred) <<
        " \"" << test_output_impl(u) << "\" (" <<
        expr1 << " " <<
        lwt_predicate_name(pred) <<
        " " << expr2 << ")";
    any_runner::instance().test(
        false, ss.str().c_str(), func, file, line);
    return false;
}

#if defined(__clang__) && defined(__has_warning)
# if __has_warning("-Wsign-compare")
#  pragma clang diagnostic pop
# endif
#elif defined(_MSC_VER)
# pragma warning(pop)
#elif defined(__GNUC__) && !(defined(__INTEL_COMPILER) || defined(__ICL) || defined(__ICC) || defined(__ECC)) && (__GNUC__ * 100 + __GNUC_MINOR__) >= 406
# pragma GCC diagnostic pop
#endif

//------------------------------------------------

struct log_type
{
    template<class T>
    friend
    std::ostream&
    operator<<(
        log_type const&, T&& t)
    {
        return any_runner::instance().log() << t;
    }
};

//------------------------------------------------

} // detail

/** Log output to the current suite
*/
constexpr detail::log_type log{};

#define BOOST_TEST(expr) ( \
    ::test_suite::detail::test_impl( \
        (expr) ? true : false, #expr, \
            BOOST_CURRENT_FUNCTION, __FILE__, __LINE__ ) )

#define BOOST_ERROR(msg) \
    ::test_suite::detail::test_impl( \
        false, msg, BOOST_CURRENT_FUNCTION, __FILE__, __LINE__ )

#define BOOST_TEST_WITH(expr1,expr2,predicate) ( \
    ::test_suite::detail::test_with_impl( \
        predicate, #expr1, #expr2, BOOST_CURRENT_FUNCTION, \
        __FILE__, __LINE__, expr1, expr2) )

#define BOOST_TEST_EQ(expr1,expr2) \
    BOOST_TEST_WITH( expr1, expr2, \
        ::test_suite::detail::lw_test_eq() )

#define BOOST_TEST_CSTR_EQ(expr1,expr2) \
    BOOST_TEST_EQ( string_view(expr1), string_view(expr2) )

#define BOOST_TEST_NE(expr1,expr2) \
    BOOST_TEST_WITH( expr1, expr2, \
        ::test_suite::detail::lw_test_ne() )

#define BOOST_TEST_LT(expr1,expr2) \
    BOOST_TEST_WITH( expr1, expr2, \
        ::test_suite::detail::lw_test_lt() )

#define BOOST_TEST_LE(expr1,expr2) \
    BOOST_TEST_WITH( expr1, expr2, \
        ::test_suite::detail::lw_test_le() )

#define BOOST_TEST_GT(expr1,expr2) \
    BOOST_TEST_WITH( expr1, expr2, \
        ::test_suite::detail::lw_test_gt() )

#define BOOST_TEST_GE(expr1,expr2) \
    BOOST_TEST_WITH( expr1, expr2, \
        ::test_suite::detail::lw_test_ge() )

#define BOOST_TEST_PASS() BOOST_TEST(true)

#define BOOST_TEST_FAIL() BOOST_TEST(false)

#define BOOST_TEST_NOT(expr) BOOST_TEST(!(expr))

#ifndef BOOST_NO_EXCEPTIONS
# define BOOST_TEST_THROWS( expr, ex ) \
    try { \
        (void)(expr); \
        ::test_suite::detail::throw_failed_impl( \
            #expr, #ex, BOOST_CURRENT_FUNCTION, \
            __FILE__, __LINE__); \
    } \
    catch(ex const&) { \
        BOOST_TEST_PASS(); \
    } \
    catch(...) { \
        ::test_suite::detail::throw_failed_impl( \
            #expr, #ex, BOOST_CURRENT_FUNCTION, \
            __FILE__, __LINE__); \
    }
   //
#else
   #define BOOST_TEST_THROWS( expr, ex )
#endif

#ifndef BOOST_NO_EXCEPTIONS
# define BOOST_TEST_NO_THROW( expr ) \
    try { \
        (void)(expr); \
        BOOST_TEST_PASS(); \
    } catch (const std::exception& e) { \
        ::test_suite::detail::no_throw_failed_impl( \
            #expr, e.what(), BOOST_CURRENT_FUNCTION, \
            __FILE__, __LINE__); \
    } catch (...) { \
        ::test_suite::detail::no_throw_failed_impl( \
            #expr, BOOST_CURRENT_FUNCTION, \
            __FILE__, __LINE__); \
    }
    //
#else
# define BOOST_TEST_NO_THROW( expr ) ( [&]{ expr; return true; }() )
#endif

/** Defines a test suite for unit testing.

    This macro creates a static instance of a `test_suite::suite` object,
    which registers a test suite with the given type and name in the global
    list of test suites.

    The `run()` method of the specified type will be called when the test
    suite is executed.

    The test suite will automatically be added to the global list of
    test suites managed by `test_suite::suites`.

    @param type The class type that implements the test suite. This class must
                have a `run()` method that contains the test logic.
    @param name A string literal representing the name of the test suite.

    @note The `type` class must be default-constructible.

    @code
    struct my_test {
        void run() {
            BOOST_TEST(1 + 1 == 2);
        }
    };

    TEST_SUITE(my_test, "My Test Suite");
    @endcode

 */
#define TEST_SUITE(type, name) \
    static ::test_suite::suite<type> type##_(name)

} // test_suite

#endif

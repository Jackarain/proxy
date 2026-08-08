//
// Copyright (c) 2025 Gennaro Prota (gennaro dot prota at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/static_string
//

#include "static_cstring.hpp"

#include <boost/core/lightweight_test.hpp>
#include <climits>
#include <cstring>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

namespace boost {
namespace static_strings {

static
void
testCStringSizeGuarantee()
{
    // Core guarantee: sizeof is exactly N + 1 (no size member).
    static_assert(sizeof(static_cstring<0>) == 1, "");
    static_assert(sizeof(static_cstring<1>) == 2, "");
    static_assert(sizeof(static_cstring<10>) == 11, "");
    static_assert(sizeof(static_cstring<63>) == 64, "");
    static_assert(sizeof(static_cstring<64>) == 65, "");
    static_assert(sizeof(static_cstring<127>) == 128, "");
    static_assert(sizeof(static_cstring<UCHAR_MAX>) == (UCHAR_MAX + 1), "");
    static_assert(sizeof(static_cstring<UCHAR_MAX + 1>) == (UCHAR_MAX + 2), "");
    static_assert(sizeof(static_cstring<1000>) == 1001, "");
}

static
void
testCStringRemainingCapacityTrick()
{
    // Test the remaining-capacity optimization for N <= UCHAR_MAX.

    // Full capacity: Last byte is both null terminator AND remaining == 0.
    {
        static_cstring<5> full("12345");
        BOOST_TEST(full.size() == 5);
        BOOST_TEST(full.capacity() == 5);
        BOOST_TEST(full.data()[5] == '\0');
        BOOST_TEST(std::strcmp(full.c_str(), "12345") == 0);
    }

    // Partial fill: Null terminator at position size, remaining at position N.
    {
        static_cstring<10> partial("Hi");
        BOOST_TEST(partial.size() == 2);
        BOOST_TEST(partial.capacity() == 10);
        BOOST_TEST(partial.data()[2] == '\0');
        BOOST_TEST(static_cast<unsigned char>(partial.data()[10]) == 8);
    }

    // Empty string.
    {
        static_cstring<10> empty;
        BOOST_TEST(empty.size() == 0);
        BOOST_TEST(empty.capacity() == 10);
        BOOST_TEST(empty.data()[0] == '\0');
        BOOST_TEST(static_cast<unsigned char>(empty.data()[10]) == 10);
    }

    // Edge case: N == UCHAR_MAX (max for our trick).
    {
        static_cstring<UCHAR_MAX> large;
        large.assign(100, 'x');
        BOOST_TEST(large.size() == 100);
        BOOST_TEST(large.capacity() == UCHAR_MAX);
        BOOST_TEST(static_cast<unsigned char>(large.data()[UCHAR_MAX]) == (UCHAR_MAX - large.size()));
    }
}

template<typename S>
struct CStringTypeTraits
{
    static_assert(std::is_trivially_copyable<S>::value);
    static_assert(std::is_trivially_copy_constructible<S>::value);
    static_assert(std::is_trivially_move_constructible<S>::value);
    static_assert(std::is_trivially_copy_assignable<S>::value);
    static_assert(std::is_trivially_move_assignable<S>::value);
    static_assert(std::is_trivially_destructible<S>::value);
};

static
void
testCStringTypeTraits()
{
    {
        using S = static_cstring<0>;
        CStringTypeTraits< S > check;
        static_cast<void>(check);
    }
    {
        using S = static_cstring<63>;
        CStringTypeTraits< S > check;
        static_cast<void>(check);
    }
    {
        using S = static_cstring<300>;
        CStringTypeTraits< S > check;
        static_cast<void>(check);
    }
}

static
void
testCStringConstruct()
{
    // Default construction.
    {
        static_cstring<1> s;
        BOOST_TEST(s.empty());
        BOOST_TEST(s.size() == 0);
        BOOST_TEST(s == "");
        BOOST_TEST(*s.end() == 0);
    }

    // Construct with count and char.
    {
        static_cstring<4> s1(3, 'x');
        BOOST_TEST(!s1.empty());
        BOOST_TEST(s1.size() == 3);
        BOOST_TEST(s1 == "xxx");
        BOOST_TEST(*s1.end() == 0);
        BOOST_TEST_THROWS(
            (static_cstring<2>(3, 'x')),
            std::length_error);
    }

    // Construct from a C string.
    {
        static_cstring<5> s1("12345");
        BOOST_TEST(s1.size() == 5);
        BOOST_TEST(s1 == "12345");
        BOOST_TEST(*s1.end() == 0);
        BOOST_TEST_THROWS(
            (static_cstring<4>("12345")),
            std::length_error);
    }

    // Construct from a C string with count.
    {
        static_cstring<5> s1("UVXYZ", 3);
        BOOST_TEST(s1 == "UVX");
        BOOST_TEST(*s1.end() == 0);
    }

    // Construct from a C string with embedded NULs.
    {
        const char arr[] = { '1', '2', '\0', '4', '5', '\0' };
        static_cstring<5> s1(arr);
        BOOST_TEST(s1 == "12" );
    }

    // Copy construction.
    {
        static_cstring<5> s1("12345");
        static_cstring<5> s2(s1);
        BOOST_TEST(s2 == "12345");
        BOOST_TEST(*s2.end() == 0);
    }
}

static
void
testCStringAssignment()
{
    // assign(size_type count, CharT ch).
    BOOST_TEST(static_cstring<3>{}.assign(1, '*') == "*");
    BOOST_TEST(static_cstring<3>{}.assign(3, '*') == "***");
    BOOST_TEST(static_cstring<3>{"abc"}.assign(3, '*') == "***");
    BOOST_TEST_THROWS(static_cstring<1>{"a"}.assign(2, '*'), std::length_error);

    // assign(CharT const* s, size_type count).
    BOOST_TEST(static_cstring<3>{}.assign("abc", 3) == "abc");
    BOOST_TEST(static_cstring<3>{"*"}.assign("abc", 3) == "abc");
    BOOST_TEST_THROWS(static_cstring<1>{}.assign("abc", 3), std::length_error);

    // assign(CharT const* s).
    BOOST_TEST(static_cstring<3>{}.assign("abc") == "abc");
    BOOST_TEST(static_cstring<3>{"*"}.assign("abc") == "abc");
    BOOST_TEST_THROWS(static_cstring<1>{}.assign("abc"), std::length_error);

    // operator=(const CharT* s).
    {
        static_cstring<3> s1;
        s1 = "123";
        BOOST_TEST(s1 == "123");
        BOOST_TEST(*s1.end() == 0);
        static_cstring<1> s2;
        BOOST_TEST_THROWS(
            s2 = "123",
            std::length_error);
    }

    // Copy assignment.
    {
        static_cstring<3> s1("123");
        static_cstring<3> s2;
        s2 = s1;
        BOOST_TEST(s2 == "123");
        BOOST_TEST(*s2.end() == 0);
    }
}

static
void
testCStringElements()
{
    using ccs3 = static_cstring<3> const;

    // at(size_type pos).
    BOOST_TEST(static_cstring<3>{"abc"}.at(0) == 'a');
    BOOST_TEST(static_cstring<3>{"abc"}.at(2) == 'c');
    BOOST_TEST_THROWS(static_cstring<3>{""}.at(0), std::out_of_range);
    BOOST_TEST_THROWS(static_cstring<3>{"abc"}.at(4), std::out_of_range);

    // at(size_type pos) const.
    BOOST_TEST(ccs3{"abc"}.at(0) == 'a');
    BOOST_TEST(ccs3{"abc"}.at(2) == 'c');
    BOOST_TEST_THROWS(ccs3{""}.at(0), std::out_of_range);

    // operator[](size_type pos).
    BOOST_TEST(static_cstring<3>{"abc"}[0] == 'a');
    BOOST_TEST(static_cstring<3>{"abc"}[2] == 'c');
    BOOST_TEST(static_cstring<3>{"abc"}[3] == 0);
    BOOST_TEST(static_cstring<3>{""}[0] == 0);

    // front() / back().
    BOOST_TEST(static_cstring<3>{"abc"}.front() == 'a');
    BOOST_TEST(static_cstring<3>{"abc"}.back() == 'c');

    // data() / c_str().
    {
        static_cstring<3> s("123");
        BOOST_TEST(std::memcmp(s.data(), "123", 3) == 0);
        BOOST_TEST(std::memcmp(s.c_str(), "123\0", 4) == 0);
    }

    // Modification through element access.
    {
        static_cstring<5> s("12345");
        s[1] = '_';
        BOOST_TEST(s == "1_345");
        s.front() = 'A';
        BOOST_TEST(s == "A_345");
        s.back() = 'Z';
        BOOST_TEST(s == "A_34Z");
    }
}

static
void
testCStringIterators()
{
    {
        static_cstring<3> s;
        BOOST_TEST(std::distance(s.begin(), s.end()) == 0);
        s = "123";
        BOOST_TEST(std::distance(s.begin(), s.end()) == 3);
    }
    {
        static_cstring<3> const s("123");
        BOOST_TEST(std::distance(s.begin(), s.end()) == 3);
        BOOST_TEST(std::distance(s.cbegin(), s.cend()) == 3);
    }

    // Iteration.
    {
        static_cstring<5> s("hello");
        std::string result;
        for (const char c : s)
        {
            result += c;
        }
        BOOST_TEST(result == "hello");
    }
}

static
void
testCStringCapacity()
{
    // empty().
    BOOST_TEST(static_cstring<0>{}.empty());
    BOOST_TEST(static_cstring<1>{}.empty());
    BOOST_TEST(!static_cstring<1>{"a"}.empty());

    // size().
    BOOST_TEST(static_cstring<0>{}.size() == 0);
    BOOST_TEST(static_cstring<1>{"a"}.size() == 1);
    BOOST_TEST(static_cstring<5>{"abc"}.size() == 3);

    // length().
    BOOST_TEST(static_cstring<0>{}.length() == 0);
    BOOST_TEST(static_cstring<3>{"abc"}.length() == 3);

    // max_size().
    BOOST_TEST(static_cstring<0>{}.max_size() == 0);
    BOOST_TEST(static_cstring<5>{"abc"}.max_size() == 5);

    // capacity().
    BOOST_TEST(static_cstring<0>{}.capacity() == 0);
    BOOST_TEST(static_cstring<5>{"abc"}.capacity() == 5);
}

static
void
testCStringClear()
{
    static_cstring<3> s("123");
    BOOST_TEST(!s.empty());
    s.clear();
    BOOST_TEST(s.empty());
    BOOST_TEST(s.size() == 0);
    BOOST_TEST(*s.end() == 0);
    BOOST_TEST(s == "");
}

static
void
testCStringPushPop()
{
    // push_back().
    {
        static_cstring<5> s("abc");
        s.push_back('d');
        BOOST_TEST(s == "abcd");
        BOOST_TEST(s.size() == 4);
        BOOST_TEST(*s.end() == 0);

        s.push_back('e');
        BOOST_TEST(s == "abcde");
        BOOST_TEST(s.size() == 5);

        BOOST_TEST_THROWS(s.push_back('f'), std::length_error);
    }

    // pop_back().
    {
        static_cstring<5> s("abcde");
        s.pop_back();
        BOOST_TEST(s == "abcd");
        BOOST_TEST(s.size() == 4);
        BOOST_TEST(*s.end() == 0);

        s.pop_back();
        s.pop_back();
        s.pop_back();
        s.pop_back();
        BOOST_TEST(s.empty());
    }
}

static
void
testCStringAppend()
{
    // append(const CharT* s).
    {
        static_cstring<12> s("Hello");
        s.append(", World");
        BOOST_TEST(s == "Hello, World");
        BOOST_TEST(*s.end() == 0);
    }
    {
        static_cstring<5> s("abc");
        BOOST_TEST_THROWS(s.append("def"), std::length_error);
    }

    // append(const CharT* s, size_type count)
    {
        static_cstring<10> s("abc");
        s.append("defgh", 3);
        BOOST_TEST(s == "abcdef");
        BOOST_TEST(*s.end() == 0);
    }

    // append(size_type count, CharT ch)
    {
        static_cstring<10> s("abc");
        s.append(3, 'x');
        BOOST_TEST(s == "abcxxx");
        BOOST_TEST(*s.end() == 0);
        BOOST_TEST_THROWS(s.append(5, 'y'), std::length_error);
    }

    // operator+=()
    {
        static_cstring<10> s("abc");
        s += "def";
        BOOST_TEST(s == "abcdef");
        s += 'g';
        BOOST_TEST(s == "abcdefg");
        BOOST_TEST(*s.end() == 0);
    }
}

static
void
testCStringComparison()
{
    // operator==() / operator!=().
    {
        static_cstring<10> a("abc");
        static_cstring<10> b("abc");
        static_cstring<10> c("abd");

        BOOST_TEST(a == b);
        BOOST_TEST(!(a == c));
        BOOST_TEST(a != c);
        BOOST_TEST(!(a != b));
    }

    // operator<(), <=(), >(), >=().
    {
        static_cstring<10> a("abc");
        static_cstring<10> b("abd");

        BOOST_TEST(a < b);
        BOOST_TEST(a <= b);
        BOOST_TEST(a <= a);
        BOOST_TEST(b > a);
        BOOST_TEST(b >= a);
        BOOST_TEST(b >= b);
    }

    // Comparison with CharT const*.
    {
        static_cstring<10> s("hello");
        BOOST_TEST(s == "hello");
        BOOST_TEST("hello" == s);
        BOOST_TEST(s != "world");
        BOOST_TEST("world" != s);
    }

    // compare().
    {
        static_cstring<10> s("abc");
        BOOST_TEST(s.compare("abc") == 0);
        BOOST_TEST(s.compare("abd") < 0);
        BOOST_TEST(s.compare("abb") > 0);
    }

    // compare(const CharT*) must not throw when the argument is longer
    // than the static capacity.
    {
        static_cstring<3> s("abc");
        BOOST_TEST(s.compare("abcd") < 0);
        BOOST_TEST(s.compare("abcdefghijklmnop") < 0);
        BOOST_TEST(s.compare("abb") > 0);
        BOOST_TEST(s.compare("abd") < 0);
        BOOST_TEST(s.compare("abc") == 0);
        BOOST_TEST(s.compare("ab") > 0);
        BOOST_TEST(s.compare("") > 0);
    }

    // Same via operator== / operator!= with an over-long C string.
    {
        static_cstring<3> s("abc");
        BOOST_TEST(!(s == "abcd"));
        BOOST_TEST(s != "abcd");
        BOOST_TEST(!("abcd" == s));
        BOOST_TEST("abcd" != s);
    }

    // Empty static_cstring vs non-empty C string.
    {
        static_cstring<5> empty;
        BOOST_TEST(empty.compare("hello") < 0);
        BOOST_TEST(empty.compare("") == 0);
    }
}

static
void
testCStringComparisonAfterShrink()
{
    // Two semantically equal strings must compare equal, even if one of
    // them was first longer and then shrunk.

    // Small-N path (remaining-capacity trick): shrinking assign(s, n).
    {
        static_cstring<10> s("hello");
        s.assign("hi", 2);
        static_cstring<10> fresh("hi");

        BOOST_TEST(s == fresh);
        BOOST_TEST(!(s != fresh));
        BOOST_TEST(!(s < fresh));
        BOOST_TEST(!(s > fresh));
        BOOST_TEST(s <= fresh);
        BOOST_TEST(s >= fresh);
    }

    // Small-N path: shrinking assign(count, ch).
    {
        static_cstring<10> s("xxxxx");
        s.assign(2, 'y');
        static_cstring<10> fresh(2, 'y');

        BOOST_TEST(s == fresh);
    }

    // Small-N path: shrinking via operator=(const CharT*).
    {
        static_cstring<10> s("hello");
        s = "hi";
        static_cstring<10> fresh("hi");

        BOOST_TEST(s == fresh);
    }

    // Small-N path: shrinking via pop_back().
    {
        static_cstring<10> s("abcde");
        s.pop_back();
        static_cstring<10> fresh("abcd");

        BOOST_TEST(s == fresh);
    }

    // Small-N path: shrinking via clear().
    {
        static_cstring<10> s("abcde");
        s.clear();
        static_cstring<10> fresh;

        BOOST_TEST(s == fresh);
    }

    // Large-N path (N > UCHAR_MAX, no remaining-capacity trick).
    {
        static_cstring<300> s;
        s.assign(200, 'a');
        s.assign(50, 'b');
        static_cstring<300> fresh(50, 'b');

        BOOST_TEST(s == fresh);
        BOOST_TEST(!(s < fresh));
        BOOST_TEST(!(s > fresh));
    }
}

static
void
testCStringConversion()
{
    // operator string_view().
    {
        static_cstring<10> s("hello");
        std::string_view sv = s;
        BOOST_TEST(sv == "hello");
        BOOST_TEST(sv.size() == 5);
    }

    // str().
    {
        static_cstring<10> s("hello");
        std::string str = s.str();
        BOOST_TEST(str == "hello");
        BOOST_TEST(str.size() == 5);
    }
}

static
void
testCStringStream()
{
    static_cstring<10> s("hello");
    std::ostringstream oss;
    oss << s;
    BOOST_TEST(oss.str() == "hello");
}

static
void
testCStringSwap()
{
    static_cstring<10> a("hello");
    static_cstring<10> b("world");

    a.swap(b);

    BOOST_TEST(a == "world");
    BOOST_TEST(b == "hello");
}

static
void
testCStringConstexpr()
{
    // constexpr construction and operations.
    constexpr static_cstring<10> ce("test");
    static_assert(ce.size() == 4, "");
    static_assert(ce[0] == 't', "");
    static_assert(ce[1] == 'e', "");
    static_assert(ce[2] == 's', "");
    static_assert(ce[3] == 't', "");
    static_assert(!ce.empty(), "");
    static_assert(ce.max_size() == 10, "");
    static_assert(ce.capacity() == 10, "");

    constexpr static_cstring<5> ce_empty;
    static_assert(ce_empty.empty(), "");
    static_assert(ce_empty.size() == 0, "");
}

// Helper struct to test NTTP.
template<basic_static_cstring S>
struct NTTPHelper
{
    static constexpr std::size_t size() noexcept
    {
        return S.size();
    }

    static constexpr const char* c_str() noexcept
    {
        return S.c_str();
    }
};

static
void
testCStringNTTP()
{
    // Test non-type template parameter usage (C++20).

    // Test size deduction from string literals.
    BOOST_TEST(NTTPHelper<"hello">::size() == 5);
    BOOST_TEST(NTTPHelper<"">::size() == 0);
    BOOST_TEST(NTTPHelper<"test">::size() == 4);

    // Test that different strings create different types
    constexpr bool same_type = std::is_same_v<
        NTTPHelper<"hello">,
        NTTPHelper<"hello">>;
    BOOST_TEST(same_type);

    constexpr bool different_type = !std::is_same_v<
        NTTPHelper<"hello">,
        NTTPHelper<"world">>;
    BOOST_TEST(different_type);

    // Test constexpr access.
    constexpr std::size_t len = NTTPHelper<"constexpr">::size();
    static_assert(len == 9, "");
}

static
void
testCStringPODUsage()
{
    // Test usage in POD types (the original motivation).
    struct UserRecord
    {
        int id;
        static_cstring<63> name;
        unsigned int flags;
    };

    static_assert(sizeof(static_cstring<63>) == 64, "");
    static_assert(std::is_trivially_copyable<UserRecord>::value, "");

    UserRecord user{};
    user.id = 42;
    user.name = "Alice";
    user.flags = 0xFF;

    BOOST_TEST(user.id == 42);
    BOOST_TEST(user.name.size() == 5);
    BOOST_TEST(user.name == "Alice");
    BOOST_TEST(user.flags == 0xFF);

    // Copy the struct.
    UserRecord copy = user;
    BOOST_TEST(copy.name == "Alice");

    // memcpy() should work.
    UserRecord memcpy_dest;
    std::memcpy(&memcpy_dest, &user, sizeof(UserRecord));
    BOOST_TEST(memcpy_dest.name == "Alice");
}

static
void
testCStringLargeCapacity()
{
    // Test strings with N > UCHAR_MAX (no remaining-capacity trick).
    {
        static_cstring<300> s;
        BOOST_TEST(s.empty());
        BOOST_TEST(s.size() == 0);

        s = "This is a test string";
        BOOST_TEST(s.size() == 21);
        BOOST_TEST(s == "This is a test string");

        s.clear();
        BOOST_TEST(s.empty());
    }

    // Large string operations.
    {
        static_cstring<500> s;
        s.assign(400, 'x');
        BOOST_TEST(s.size() == 400);
        BOOST_TEST(s[0] == 'x');
        BOOST_TEST(s[399] == 'x');
        BOOST_TEST(*s.end() == 0);
    }
}

static
void
testCStringWideChar()
{
    // Test with wchar_t.
    {
        static_wcstring<10> ws;
        BOOST_TEST(ws.empty());
        BOOST_TEST(ws.size() == 0);
    }

    {
        static_wcstring<10> ws(L"hello");
        BOOST_TEST(ws.size() == 5);
        BOOST_TEST(ws[0] == L'h');
        BOOST_TEST(ws[4] == L'o');
        BOOST_TEST(*ws.end() == 0);
    }
}

int
runTests()
{
  testCStringSizeGuarantee();
  testCStringRemainingCapacityTrick();
  testCStringTypeTraits();
  testCStringConstruct();
  testCStringAssignment();
  testCStringElements();
  testCStringIterators();
  testCStringCapacity();
  testCStringClear();
  testCStringPushPop();
  testCStringAppend();
  testCStringComparison();
  testCStringComparisonAfterShrink();
  testCStringConversion();
  testCStringStream();
  testCStringSwap();
  testCStringConstexpr();
  testCStringNTTP();
  testCStringPODUsage();
  testCStringLargeCapacity();
  testCStringWideChar();

  return report_errors();
}
}
}

int
main()
{
  return boost::static_strings::runTests();
}

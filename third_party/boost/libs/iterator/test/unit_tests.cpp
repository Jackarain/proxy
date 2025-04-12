// Copyright David Abrahams 2003.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/iterator/iterator_adaptor.hpp>

#include "static_assert_same.hpp"

#include <iterator>
#include <type_traits>
#include <boost/iterator/minimum_category.hpp>

struct input_output_iterator_tag :
    public std::input_iterator_tag
{
    // Using inheritance for only input_iterator_tag helps to avoid
    // ambiguities when a stdlib implementation dispatches on a
    // function which is overloaded on both input_iterator_tag and
    // output_iterator_tag, as STLPort does, in its __valid_range
    // function.  I claim it's better to avoid the ambiguity in these
    // cases.
    operator std::output_iterator_tag() const
    {
        return std::output_iterator_tag();
    }
};

struct X { int a; };


struct Xiter : boost::iterator_adaptor<Xiter,X*>
{
    Xiter();
    Xiter(X* p) : boost::iterator_adaptor<Xiter, X*>(p) {}
};

void take_xptr(X*) {}
void operator_arrow_test()
{
    // check that the operator-> result is a pointer for lvalue iterators
    X x;
    take_xptr(Xiter(&x).operator->());
}

template <class T, class U, class Min>
struct static_assert_min_cat
  : static_assert_same<
       typename boost::iterators::minimum_category<T,U>::type, Min
    >
{};

void category_test()
{
    using namespace boost::iterators;
    using namespace boost::iterators::detail;

    static_assert(
        !std::is_convertible<std::input_iterator_tag, input_output_iterator_tag>::value,
        "std::input_iterator_tag is not expected to be convertible to input_output_iterator_tag.");

    static_assert(
        !std::is_convertible<std::output_iterator_tag, input_output_iterator_tag>::value,
        "std::output_iterator_tag is not expected to be convertible to input_output_iterator_tag.");

    static_assert(
        std::is_convertible<input_output_iterator_tag, std::input_iterator_tag>::value,
        "input_output_iterator_tag is expected to be convertible to std::input_iterator_tag.");

    static_assert(
        std::is_convertible<input_output_iterator_tag, std::output_iterator_tag>::value,
        "input_output_iterator_tag is expected to be convertible to std::output_iterator_tag.");

#if 0 // This seems wrong; we're not advertising
      // input_output_iterator_tag are we?
    static_assert(
        boost::is_convertible<
            std::forward_iterator_tag
          , input_output_iterator_tag
        >::value,
        "");
#endif

    int test = static_assert_min_cat<
        std::input_iterator_tag, input_output_iterator_tag, std::input_iterator_tag
    >::value;

    test = static_assert_min_cat<
        input_output_iterator_tag, std::input_iterator_tag, std::input_iterator_tag
    >::value;

#if 0
    test = static_assert_min_cat<
        input_output_iterator_tag, std::forward_iterator_tag, input_output_iterator_tag
    >::value;
#endif

    test = static_assert_min_cat<
        std::input_iterator_tag, std::forward_iterator_tag, std::input_iterator_tag
    >::value;

    test = static_assert_min_cat<
        std::input_iterator_tag, std::random_access_iterator_tag, std::input_iterator_tag
    >::value;

#if 0  // This would be wrong: a random access iterator is not
       // neccessarily writable, as is an output iterator.
    test = static_assert_min_cat<
        std::output_iterator_tag, std::random_access_iterator_tag, std::output_iterator_tag
    >::value;
#endif

    (void)test;
}

int main()
{
    category_test();
    operator_arrow_test();
    return 0;
}

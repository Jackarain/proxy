// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <any>

#include <boost/openmethod.hpp>
#include <boost/openmethod/interop/std_shared_ptr.hpp>
#include <boost/openmethod/interop/std_unique_ptr.hpp>
#include <boost/openmethod/initialize.hpp>
#include <boost/openmethod/policies/vptr_vector.hpp>

#include "test_util.hpp"

#define BOOST_TEST_MODULE openmethod
#include <boost/test/unit_test.hpp>

using namespace boost::openmethod;

namespace TEST_NS {

using namespace test_matrices;

struct test_registry : test_registry_<__COUNTER__> {};

BOOST_OPENMETHOD_CLASSES(matrix, dense_matrix, test_registry);

BOOST_OPENMETHOD(
    times, (virtual_<const matrix&>, virtual_<const matrix&>),
    std::unique_ptr<matrix>, test_registry);

BOOST_OPENMETHOD_OVERRIDE(
    times, (const dense_matrix&, const matrix&),
    std::unique_ptr<dense_matrix>) {
    return std::make_unique<dense_matrix>();
}

BOOST_OPENMETHOD_OVERRIDE(
    times, (const matrix&, const dense_matrix&), std::unique_ptr<matrix>) {
    return std::make_unique<matrix>();
}

static_assert(
    std::is_same_v<
        detail::virtual_type<std::unique_ptr<matrix>, test_registry>, matrix>);

BOOST_AUTO_TEST_CASE(covariant_return_type) {
    auto compiler = boost::openmethod::initialize<test_registry>(n2216());
    BOOST_TEST(compiler.report.ambiguous == 0u);

    // test_registry: use covariant return types to resolve ambiguity.
    dense_matrix left, right;
    auto result = times(left, right);
    BOOST_TEST(result->type == DENSE_MATRIX);
}

} // namespace TEST_NS

namespace TEST_NS {

using namespace test_matrices;

struct test_registry : test_registry_<__COUNTER__> {};

BOOST_OPENMETHOD_CLASSES(matrix, dense_matrix, test_registry);

BOOST_OPENMETHOD(
    times, (virtual_<const matrix&>, virtual_<const matrix&>), string_pair,
    test_registry);

BOOST_OPENMETHOD_OVERRIDE(times, (const matrix&, const matrix&), string_pair) {
    return string_pair(MATRIX_MATRIX, NONE);
}

BOOST_OPENMETHOD_OVERRIDE(
    times, (const dense_matrix&, const matrix&), string_pair) {
    return string_pair(DENSE_MATRIX, MATRIX_MATRIX);
}

BOOST_OPENMETHOD_OVERRIDE(
    times, (const matrix&, const dense_matrix&), string_pair) {
    return string_pair(MATRIX_DENSE, MATRIX_MATRIX);
}

BOOST_AUTO_TEST_CASE(pick_any_ambiguous) {
    auto compiler = boost::openmethod::initialize<test_registry>(n2216());
    BOOST_TEST(compiler.report.ambiguous == 1u);

    // test_registry: use covariant return types to resolve ambiguity.
    dense_matrix left, right;
    auto result = times(left, right);
    BOOST_TEST(result.first == MATRIX_DENSE);
    BOOST_TEST(result.second == MATRIX_MATRIX);
}

} // namespace TEST_NS

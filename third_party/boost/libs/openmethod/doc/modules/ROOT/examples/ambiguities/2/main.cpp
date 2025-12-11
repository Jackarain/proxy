// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// clang-format off

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>
#include <iostream>

struct Matrix { virtual ~Matrix() = default; };
struct DenseMatrix : Matrix {};
struct SparseMatrix : Matrix {};

BOOST_OPENMETHOD_CLASSES(Matrix, DenseMatrix, SparseMatrix);

using boost::openmethod::virtual_ptr;

BOOST_OPENMETHOD(
    add, (virtual_ptr<const Matrix>, virtual_ptr<const Matrix>), void);

BOOST_OPENMETHOD_OVERRIDE(
    add, (virtual_ptr<const Matrix>, virtual_ptr<const SparseMatrix>), void) {
}

BOOST_OPENMETHOD_OVERRIDE(
    add, (virtual_ptr<const SparseMatrix>, virtual_ptr<const Matrix>), void) {
}

// tag::content[]
BOOST_OPENMETHOD_OVERRIDE(
    add, (virtual_ptr<const SparseMatrix>, virtual_ptr<const SparseMatrix>), void) {
}
// end::content[]

int main() {
    boost::openmethod::initialize();

    SparseMatrix a, b;
    add(a, b);
}

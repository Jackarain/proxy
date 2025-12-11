// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <memory>
#include <string>
#include <typeinfo>

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>
#include <boost/openmethod/interop/std_shared_ptr.hpp>

using std::make_shared;
using std::shared_ptr;
using std::string;

using boost::openmethod::make_shared_virtual;
using boost::openmethod::shared_virtual_ptr;
using boost::openmethod::virtual_ptr;

struct matrix {
    virtual ~matrix() {
    }
    virtual auto at(int row, int col) const -> double = 0;
    // ...
};

struct dense_matrix : matrix {
    virtual auto at(int /*row*/, int /*col*/) const -> double {
        return 0;
    }
};

struct diagonal_matrix : matrix {
    virtual auto at(int /*row*/, int /*col*/) const -> double {
        return 0;
    }
};

BOOST_OPENMETHOD_CLASSES(matrix, dense_matrix, diagonal_matrix);

BOOST_OPENMETHOD(to_json, (virtual_ptr<const matrix>), string);

BOOST_OPENMETHOD_OVERRIDE(to_json, (virtual_ptr<const dense_matrix>), string) {
    return "json for dense matrix...";
}

BOOST_OPENMETHOD_OVERRIDE(
    to_json, (virtual_ptr<const diagonal_matrix>), string) {
    return "json for diagonal matrix...";
}

// -----------------------------------------------------------------------------
// matrix * matrix

BOOST_OPENMETHOD(
    times, (shared_virtual_ptr<const matrix>, shared_virtual_ptr<const matrix>),
    shared_virtual_ptr<const matrix>);

// catch-all matrix * matrix -> dense_matrix
BOOST_OPENMETHOD_OVERRIDE(
    times,
    (shared_virtual_ptr<const matrix> /*a*/,
     shared_virtual_ptr<const matrix> /*b*/),
    shared_virtual_ptr<const dense_matrix>) {
    return make_shared<const dense_matrix>();
}

// diagonal_matrix * diagonal_matrix -> diagonal_matrix
BOOST_OPENMETHOD_OVERRIDE(
    times,
    (shared_virtual_ptr<const diagonal_matrix> /*a*/,
     shared_virtual_ptr<const diagonal_matrix> /*b*/),
    shared_virtual_ptr<const diagonal_matrix>) {
    return make_shared_virtual<diagonal_matrix>();
}

inline auto operator*(shared_ptr<const matrix> a, shared_ptr<const matrix> b)
    -> shared_virtual_ptr<const matrix> {
    return times(a, b);
}

// -----------------------------------------------------------------------------
// scalar * matrix

BOOST_OPENMETHOD(
    times, (double, shared_virtual_ptr<const matrix>),
    shared_virtual_ptr<const matrix>);

// catch-all matrix * scalar -> dense_matrix
BOOST_OPENMETHOD_OVERRIDE(
    times, (double /*a*/, shared_virtual_ptr<const matrix> /*b*/),
    shared_virtual_ptr<const dense_matrix>) {
    return make_shared_virtual<dense_matrix>();
}

BOOST_OPENMETHOD_OVERRIDE(
    times, (double /*a*/, shared_virtual_ptr<const diagonal_matrix> /*b*/),
    shared_virtual_ptr<const diagonal_matrix>) {
    return make_shared_virtual<diagonal_matrix>();
}

// -----------------------------------------------------------------------------
// matrix * scalar

// just swap
inline auto times(shared_virtual_ptr<const matrix> a, double b)
    -> shared_virtual_ptr<const matrix> {
    return times(b, a);
}

// -----------------------------------------------------------------------------
// main

#define check(expr)                                                            \
    {                                                                          \
        if (!(expr)) {                                                         \
            cerr << #expr << " failed\n";                                      \
        }                                                                      \
    }

auto main() -> int {
    using std::cerr;
    using std::cout;

    boost::openmethod::initialize();

    shared_ptr<const matrix> a = make_shared<dense_matrix>();
    shared_ptr<const matrix> b = make_shared<diagonal_matrix>();
    double s = 1;

#ifdef BOOST_CLANG
#pragma clang diagnostic ignored "-Wpotentially-evaluated-expression"
#endif

    check(typeid(*times(a, a)) == typeid(dense_matrix));
    check(typeid(*times(a, b)) == typeid(dense_matrix));
    check(typeid(*times(b, b)) == typeid(diagonal_matrix));
    check(typeid(*times(s, a)) == typeid(dense_matrix));
    check(typeid(*times(s, b)) == typeid(diagonal_matrix));

    cout << to_json(*a) << "\n"; // json for dense matrix
    cout << to_json(*b) << "\n"; // json for diagonal matrix

    return 0;
}

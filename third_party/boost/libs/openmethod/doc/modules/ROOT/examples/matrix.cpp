// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// https://godbolt.org/z/r6o4f171r

#include <iostream>
#include <memory>
#include <string>
#include <typeinfo>

#include <boost/openmethod.hpp>
#include <boost/openmethod/policies/static_rtti.hpp>
#include <boost/openmethod/initialize.hpp>

using std::string;

using namespace boost::openmethod::aliases;

struct abstract {
    int ref_count = 0;
};

struct registry
    : boost::openmethod::registry<boost::openmethod::policies::static_rtti> {};

template<class Rep>
using matrix_ptr = boost::openmethod::virtual_ptr<Rep, registry>;

BOOST_OPENMETHOD(destroy, (matrix_ptr<abstract>), void, registry);

class matrix {
    matrix_ptr<abstract> rep_;

    explicit matrix(matrix_ptr<abstract> rep) : rep_(rep) {
        ++rep_->ref_count;

        if (--rep->ref_count == 0) {
            destroy(rep);
        }
    }

  public:
    matrix(const matrix&) = default;
    matrix& operator=(const matrix&) = default;

    auto rep() const -> matrix_ptr<abstract> {
        return rep_;
    }

    template<class Rep, class... Args>
    static matrix make(Args&&... args) {
        return matrix(
            boost::openmethod::final_virtual_ptr<registry>(
                *new Rep(std::forward<Args>(args)...)));
    }
};

struct dense : abstract {
    static constexpr const char* type = "dense";
};

BOOST_OPENMETHOD_OVERRIDE(destroy, (matrix_ptr<dense> rep), void) {
    delete rep.get();
}

struct diagonal : abstract {
    static constexpr const char* type = "diagonal";
};

BOOST_OPENMETHOD_OVERRIDE(destroy, (matrix_ptr<diagonal> rep), void) {
    delete rep.get();
}

BOOST_OPENMETHOD_CLASSES(abstract, dense, diagonal, registry);

// -----------------------------------------------------------------------------
// matrix * matrix

BOOST_OPENMETHOD(
    times, (matrix_ptr<abstract>, matrix_ptr<abstract>), matrix, registry);

// catch-all matrix * matrix -> dense
BOOST_OPENMETHOD_OVERRIDE(
    times, (matrix_ptr<abstract> /*a*/, matrix_ptr<abstract> /*b*/), matrix) {
    return matrix::make<dense>();
}

// diagonal * diagonal -> diagonal
BOOST_OPENMETHOD_OVERRIDE(
    times, (matrix_ptr<diagonal> /*a*/, matrix_ptr<diagonal> /*b*/), matrix) {
    return matrix::make<diagonal>();
}

inline auto operator*(matrix a, matrix b) -> matrix {
    return times(a.rep(), b.rep());
}

// -----------------------------------------------------------------------------
// scalar * matrix

BOOST_OPENMETHOD(times, (double, matrix_ptr<abstract>), matrix, registry);

// catch-all matrix * scalar -> dense
BOOST_OPENMETHOD_OVERRIDE(
    times, (double /*a*/, matrix_ptr<abstract> /*b*/), matrix) {
    return matrix::make<dense>();
}

BOOST_OPENMETHOD_OVERRIDE(
    times, (double /*a*/, matrix_ptr<diagonal> /*b*/), matrix) {
    return matrix::make<diagonal>();
}

// -----------------------------------------------------------------------------
// matrix * scalar

// just swap
inline auto times(matrix_ptr<abstract> a, double b) -> matrix {
    return times(b, a);
}

// -----------------------------------------------------------------------------
// main

BOOST_OPENMETHOD(write, (matrix_ptr<abstract>), string, registry);

inline auto operator<<(std::ostream& os, matrix a) -> std::ostream& {
    return os << write(a.rep());
}

BOOST_OPENMETHOD_OVERRIDE(write, (matrix_ptr<dense>), string) {
    return "a dense matrix";
}

BOOST_OPENMETHOD_OVERRIDE(write, (matrix_ptr<diagonal>), string) {
    return "a diagonal matrix";
}

auto main() -> int {
    using std::cerr;
    using std::cout;

    boost::openmethod::initialize<registry>();

    matrix a = matrix::make<dense>();
    matrix b = matrix::make<diagonal>();
    double s = 1;

    cout << a << "\n";
    cout << b << "\n";

    return 0;
}

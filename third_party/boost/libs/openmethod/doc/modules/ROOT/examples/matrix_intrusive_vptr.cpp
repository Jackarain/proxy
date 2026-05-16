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
#include <boost/openmethod/interop/boost_intrusive_ptr.hpp>

using std::string;

using namespace boost::openmethod::aliases;

struct abstract {
    virtual ~abstract() {
    }

    int ref_count = 0;

    friend void intrusive_ptr_add_ref(abstract* p) {
        ++p->ref_count;
    }

    friend void intrusive_ptr_release(abstract* p) {
        if (--p->ref_count == 0) {
            delete p;
        }
    }
};

template<class Rep>
using matrix_ptr = boost::openmethod::boost_intrusive_virtual_ptr<Rep>;

class matrix {
    matrix_ptr<abstract> rep_;

    explicit matrix(matrix_ptr<abstract> rep) : rep_(rep) {
    }

  public:
    matrix(const matrix&) = default;
    matrix& operator=(const matrix&) = default;

    auto rep() const -> matrix_ptr<abstract> {
        return rep_;
    }

    template<class Rep>
    static matrix make() {
        return matrix(boost::openmethod::make_boost_intrusive_virtual<Rep>());
    }
};

struct dense : abstract {};

struct diagonal : abstract {};

using namespace boost::openmethod::aliases;

BOOST_OPENMETHOD_CLASSES(abstract, dense, diagonal);

BOOST_OPENMETHOD(to_str, (matrix_ptr<abstract>), string);

inline auto operator<<(std::ostream& os, matrix a) -> std::ostream& {
    return os << to_str(a.rep());
}

BOOST_OPENMETHOD_OVERRIDE(to_str, (matrix_ptr<dense>), string) {
    return "a dense matrix";
}

BOOST_OPENMETHOD_OVERRIDE(to_str, (matrix_ptr<diagonal>), string) {
    return "a diagonal matrix";
}

// -----------------------------------------------------------------------------
// matrix * matrix

BOOST_OPENMETHOD(times, (matrix_ptr<abstract>, matrix_ptr<abstract>), matrix);

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

BOOST_OPENMETHOD(times, (double, matrix_ptr<abstract>), matrix);

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

    matrix a = matrix::make<dense>();
    matrix b = matrix::make<diagonal>();
    double s = 1;

    cout << a << "\n";
    cout << b << "\n";

    return 0;
}

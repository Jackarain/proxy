// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_OPENMETHOD_TEST_HELPERS_HPP
#define BOOST_OPENMETHOD_TEST_HELPERS_HPP

#include <iostream>

#include <boost/openmethod/core.hpp>
#include <boost/openmethod/initialize.hpp>

template<typename Key>
struct unique final {
    using category = unique;
    template<class Registry>
    struct fn {};
};

template<int N, class... Policies>
struct test_registry_ : boost::openmethod::default_registry::with<
                            unique<test_registry_<N>>, Policies...> {
    using registry_type = boost::openmethod::default_registry::with<
        unique<test_registry_<N>>, Policies...>;
};

#define TEST_NS BOOST_PP_CAT(test, __COUNTER__)

struct capture_cout {
    capture_cout(std::streambuf* new_buffer)
        : old(std::cout.rdbuf(new_buffer)) {
    }

    ~capture_cout() {
        std::cout.rdbuf(old);
    }

  private:
    std::streambuf* old;
};

#define MAKE_STRING_CONSTANT(ID) static const std::string ID = #ID;

struct string_pair : std::pair<std::string, std::string> {
    using std::pair<std::string, std::string>::pair;
};

inline std::ostream& operator<<(std::ostream& os, const string_pair& pair) {
    return os << "(" << pair.first << ", " << pair.second << ")";
}

namespace test_matrices {

MAKE_STRING_CONSTANT(NONE)
MAKE_STRING_CONSTANT(MATRIX)
MAKE_STRING_CONSTANT(DIAGONAL)
MAKE_STRING_CONSTANT(SCALAR_MATRIX)
MAKE_STRING_CONSTANT(SCALAR_DIAGONAL)
MAKE_STRING_CONSTANT(MATRIX_SCALAR)
MAKE_STRING_CONSTANT(DIAGONAL_SCALAR)
MAKE_STRING_CONSTANT(MATRIX_MATRIX)
MAKE_STRING_CONSTANT(MATRIX_DIAGONAL)
MAKE_STRING_CONSTANT(DIAGONAL_DIAGONAL)
MAKE_STRING_CONSTANT(DIAGONAL_MATRIX)
MAKE_STRING_CONSTANT(MATRIX_DENSE)
MAKE_STRING_CONSTANT(DENSE_MATRIX)

struct matrix {
    matrix() : type(MATRIX) {
    }

    virtual ~matrix() {
    }

    std::string type;

  protected:
    matrix(std::string type) : type(std::move(type)) {
    }
};

struct dense_matrix : matrix {
    dense_matrix() : matrix(DENSE_MATRIX) {
    }
};

struct diagonal_matrix : matrix {
    diagonal_matrix() : matrix(DIAGONAL_MATRIX) {
    }
};

} // namespace test_matrices

#endif

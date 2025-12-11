// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/openmethod.hpp>

using namespace boost::openmethod;

struct A {
    virtual ~A() = default;
};

struct B1 : A {};
struct B2 : A {};
struct C : B1, B2 {};

BOOST_OPENMETHOD_CLASSES(A, B1, B2, C);

int main() {
}

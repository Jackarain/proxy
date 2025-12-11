// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// clang-format off

// tag::content[]
struct Node {
    virtual ~Node() {}
    virtual int value() const = 0;
};

struct Variable : Node {
    Variable(int value) : v(value) {}
    int value() const override { return v; }
    int v;
};

struct Plus : Node {
    Plus(const Node& left, const Node& right) : left(left), right(right) {}
    int value() const override { return left.value() + right.value(); }
    const Node& left; const Node& right;
};

struct Times : Node {
    Times(const Node& left, const Node& right) : left(left), right(right) {}
    int value() const override { return left.value() * right.value(); }
    const Node& left; const Node& right;
};

// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
// library code
// =============================================================================
// application code
// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>
#include <iostream>

using boost::openmethod::virtual_ptr;

BOOST_OPENMETHOD(postfix, (virtual_ptr<const Node> node, std::ostream& os), void);

// tag::variable_overrider[]
BOOST_OPENMETHOD_OVERRIDE(
    postfix, (virtual_ptr<const Variable> var, std::ostream& os), void) {
    os << var->v;
}
// end::variable_overrider[]

// tag::plus_overrider[]
BOOST_OPENMETHOD_OVERRIDE(
    postfix, (virtual_ptr<const Plus> plus, std::ostream& os), void) {
    postfix(plus->left, os);
    os << ' ';
    postfix(plus->right, os);
    os << " +";
}
// end::plus_overrider[]

BOOST_OPENMETHOD_OVERRIDE(
    postfix, (virtual_ptr<const Times> times, std::ostream& os), void) {
    postfix(times->left, os);
    os << ' ';
    postfix(times->right, os);
    os << " *";
}

// tag::class_registration[]
BOOST_OPENMETHOD_CLASSES(Node, Variable, Plus, Times);
// end::class_registration[]

int main() {
    boost::openmethod::initialize();
    Variable a{2}, b{3}, c{4};
    Plus d{a, b}; Times e{d, c};
    postfix(e, std::cout);
    std::cout << " = " << e.value() << "\n"; // 2 3 + 4 * = 20
}
// end::content[]

void call_via_ref(const Node& node, std::ostream& os) {
    postfix(node, os);
}

void call_via_virtual_ptr(virtual_ptr<const Node> node, std::ostream& os) {
    postfix(node, os);
}

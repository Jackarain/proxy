// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>

// tag::content[]
struct Node {
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

// tag::content[]
#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>

using boost::openmethod::virtual_;

BOOST_OPENMETHOD(postfix, (virtual_<const Node&> node, std::ostream& os), void);

BOOST_OPENMETHOD_OVERRIDE(
    postfix, (const Variable& var, std::ostream& os), void) {
    os << var.v;
}

BOOST_OPENMETHOD_OVERRIDE(
    postfix, (const Plus& plus, std::ostream& os), void) {
    postfix(plus.left, os);
    os << ' ';
    postfix(plus.right, os);
    os << " +";
}

BOOST_OPENMETHOD_OVERRIDE(
    postfix, (const Times& times, std::ostream& os), void) {
    postfix(times.left, os);
    os << ' ';
    postfix(times.right, os);
    os << " *";
}

BOOST_OPENMETHOD_CLASSES(Node, Variable, Plus, Times);

auto main() -> int {
    boost::openmethod::initialize();
    Variable a{2}, b{3}, c{4};
    Plus d{a, b};
    Times e{d, c};
    postfix(e, std::cout);
    std::cout << " = " << e.value() << "\n"; // 2 3 + 4 * = 20
}
// end::content[]

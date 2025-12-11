// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// clang-format off

// tag::content[]
#include <boost/openmethod.hpp>
#include <boost/openmethod/interop/std_unique_ptr.hpp>

using namespace boost::openmethod::aliases;

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
    Plus(unique_virtual_ptr<const Node> left, unique_virtual_ptr<const Node> right)
        : left(std::move(left)), right(std::move(right)) {}
    int value() const override { return left->value() + right->value(); }
    unique_virtual_ptr<const Node> left, right;
};

struct Times : Node {
    Times(unique_virtual_ptr<const Node> left, unique_virtual_ptr<const Node> right)
        : left(std::move(left)), right(std::move(right)) {}
    int value() const override { return left->value() * right->value(); }
    unique_virtual_ptr<const Node> left, right;
};

#include <iostream>

BOOST_OPENMETHOD(postfix, (virtual_ptr<const Node> node, std::ostream& os), void);

BOOST_OPENMETHOD_OVERRIDE(
    postfix, (virtual_ptr<const Variable> var, std::ostream& os), void) {
    os << var->v;
}

BOOST_OPENMETHOD_OVERRIDE(
    postfix, (virtual_ptr<const Plus> plus, std::ostream& os), void) {
    postfix(plus->left, os);
    os << ' ';
    postfix(plus->right, os);
    os << " +";
}

BOOST_OPENMETHOD_OVERRIDE(
    postfix, (virtual_ptr<const Times> times, std::ostream& os), void) {
    postfix(times->left, os);
    os << ' ';
    postfix(times->right, os);
    os << " *";
}

BOOST_OPENMETHOD_CLASSES(Node, Variable, Plus, Times);

#include <boost/openmethod/initialize.hpp>

int main() {
    boost::openmethod::initialize();

    auto a = std::make_unique<Variable>(2);
    auto b = std::make_unique<Variable>(3);
    auto c = std::make_unique<Variable>(4);
    auto d = make_unique_virtual<Plus>(std::move(a), std::move(b));
    auto e = make_unique_virtual<Times>(std::move(d), std::move(c));

    postfix(e, std::cout);
    std::cout << " = " << e->value() << "\n"; // 2 3 + 4 * = 20
}
// end::content[]

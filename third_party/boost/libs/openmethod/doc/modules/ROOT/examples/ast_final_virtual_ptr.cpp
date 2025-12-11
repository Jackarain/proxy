// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// clang-format off

// tag::content[]
#include <boost/openmethod.hpp>
using namespace boost::openmethod::aliases;

struct Node {
};

struct Variable : Node {
    Variable(int value) : v(value) {}
    int v;
};

struct Plus : Node {
    Plus(virtual_ptr<const Node> left, virtual_ptr<const Node> right)
        : left(left), right(right) {}
    virtual_ptr<const Node> left, right;
};

struct Times : Node {
    Times(virtual_ptr<const Node> left, virtual_ptr<const Node> right)
        : left(left), right(right) {}
    virtual_ptr<const Node> left, right;
};

#include <iostream>

using boost::openmethod::virtual_ptr;

BOOST_OPENMETHOD(value, (virtual_ptr<const Node>), int);

BOOST_OPENMETHOD_OVERRIDE(value, (virtual_ptr<const Variable> node), int) {
    return node->v;
}

BOOST_OPENMETHOD_OVERRIDE(value, (virtual_ptr<const Plus> node), int) {
    return value(node->left) + value(node->right);
}

BOOST_OPENMETHOD_OVERRIDE(value, (virtual_ptr<const Times> node), int) {
    return value(node->left) * value(node->right);
}

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
    Variable a{2}, b{3}, c{4};
    Plus d{final_virtual_ptr(a), final_virtual_ptr(b)};
    Times e{final_virtual_ptr(d), final_virtual_ptr(c)};
    auto root = final_virtual_ptr(e);
    postfix(root, std::cout);
    std::cout << " = " << value(root) << "\n"; // 2 3 + 4 * = 20
}
// end::content[]

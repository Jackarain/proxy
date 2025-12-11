// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// clang-format off

#include <iostream>

// tag::classes[]
#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>

struct Node {
    virtual ~Node() {}
    virtual int value() const = 0;
};

struct Variable : Node {
    Variable(int value) : v(value) {}
    int value() const override { return v; }
    int v;
};

template<typename Op>
struct BinaryOp : Node {
    BinaryOp(const Node& left, const Node& right) : left(left), right(right) {}
    int value() const override { return Op()(left.value(), right.value()); }
    const Node& left;
    const Node& right;
};
// end::classes[]

using namespace boost::openmethod;

// tag::method[]
BOOST_OPENMETHOD(
    postfix, (virtual_ptr<const Node> node, std::ostream& os), void);

BOOST_OPENMETHOD_OVERRIDE(
    postfix, (virtual_ptr<const Variable> var, std::ostream& os), void) {
    os << var->v;
}
// end::method[]

// tag::postfix_binary[]
template<class BinaryOp, char Op>
auto postfix_binary(virtual_ptr<const BinaryOp> node, std::ostream& os) {
    postfix(node->left, os);
    os << ' ';
    postfix(node->right, os);
    os << " " << Op;
}
// end::postfix_binary[]

// tag::add_postfix_binary[]
BOOST_OPENMETHOD_TYPE(
    postfix, (virtual_ptr<const Node> node, std::ostream& os), void)::
    override<
        postfix_binary<BinaryOp<std::plus<int>>, '+'>,
        postfix_binary<
            BinaryOp<std::multiplies<int>>, '*'>> add_binary_overriders;
// end::add_postfix_binary[]

// tag::main[]
BOOST_OPENMETHOD_CLASSES(
    Node, Variable, BinaryOp<std::plus<int>>, BinaryOp<std::multiplies<int>>);

auto main() -> int {
    boost::openmethod::initialize();
    Variable a{2}, b{3}, c{4};
    BinaryOp<std::plus<int>> d{a, b};
    BinaryOp<std::multiplies<int>> e{d, c};
    postfix(e, std::cout);
    std::cout << " = " << e.value() << "\n"; // 2 3 + 4 * = 20
}
// end::main[]

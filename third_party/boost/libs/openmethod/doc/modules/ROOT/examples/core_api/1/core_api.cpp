// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>

#include <boost/openmethod/initialize.hpp>
#include <boost/openmethod/core.hpp>

struct Node {
    virtual ~Node() {
    }
    virtual int value() const = 0;
};

struct Variable : Node {
    Variable(int value) : v(value) {
    }
    int value() const override {
        return v;
    }
    int v;
};

struct Plus : Node {
    Plus(const Node& left, const Node& right) : left(left), right(right) {
    }
    int value() const override {
        return left.value() + right.value();
    }
    const Node& left;
    const Node& right;
};

struct Times : Node {
    Times(const Node& left, const Node& right) : left(left), right(right) {
    }
    int value() const override {
        return left.value() * right.value();
    }
    const Node& left;
    const Node& right;
};

using namespace boost::openmethod;

// tag::method[]
#include <boost/openmethod/macros.hpp>

struct BOOST_OPENMETHOD_ID(postfix);

using postfix = method<
    BOOST_OPENMETHOD_ID(postfix),
    void(virtual_ptr<const Node> node, std::ostream& os)>;
// end::method[]

// tag::variable_overrider[]
auto postfix_variable(virtual_ptr<const Variable> node, std::ostream& os) {
    os << node->v;
}

static postfix::override<postfix_variable> override_postfix_variable;
// end::variable_overrider[]

// tag::binary_overriders[]
#include <boost/openmethod/macros.hpp>

auto postfix_plus(virtual_ptr<const Plus> node, std::ostream& os) {
    postfix::fn(node->left, os);
    os << ' ';
    postfix::fn(node->right, os);
    os << " +";
}

auto postfix_times(virtual_ptr<const Times> node, std::ostream& os) {
    postfix::fn(node->left, os);
    os << ' ';
    postfix::fn(node->right, os);
    os << " *";
}

BOOST_OPENMETHOD_REGISTER(postfix::override<postfix_plus, postfix_times>);
// end::binary_overriders[]

// tag::use_classes[]
BOOST_OPENMETHOD_REGISTER(use_classes<Node, Variable, Plus, Times>);
// end::use_classes[]

// tag::main[]
auto main() -> int {
    boost::openmethod::initialize();
    Variable a{2}, b{3}, c{4};
    Plus d{a, b};
    Times e{d, c};
    postfix::fn(e, std::cout);
    std::cout << " = " << e.value() << "\n"; // 2 3 + 4 * = 20
}
// end::main[]

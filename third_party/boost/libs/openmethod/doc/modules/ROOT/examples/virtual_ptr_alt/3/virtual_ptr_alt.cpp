// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <boost/openmethod.hpp>
#include <boost/openmethod/inplace_vptr.hpp>

// tag::content[]
struct Node : boost::openmethod::inplace_vptr_base<Node> {
};

struct Variable : Node, boost::openmethod::inplace_vptr_derived<Variable, Node> {
    Variable(int value) : v(value) {}

    int v;
};

struct Plus : Node, boost::openmethod::inplace_vptr_derived<Plus, Node> {
    Plus(const Node& left, const Node& right) : left(left), right(right) {}

    const Node& left;
    const Node& right;
};

struct Times : Node, boost::openmethod::inplace_vptr_derived<Times, Node> {
    Times(const Node& left, const Node& right) : left(left), right(right) {}

    const Node& left;
    const Node& right;
};
// end::content[]

#include <boost/openmethod/initialize.hpp>

using boost::openmethod::virtual_;

BOOST_OPENMETHOD(value, (virtual_<const Node&> node), int);

BOOST_OPENMETHOD_OVERRIDE(value, (const Variable& var), int) {
    return var.v;
}

BOOST_OPENMETHOD_OVERRIDE(value, (const Plus& plus), int) {
    return value(plus.left) + value(plus.right);
}

BOOST_OPENMETHOD_OVERRIDE(value, (const Times& times), int) {
    return value(times.left) * value(times.right);
}

auto main() -> int {
    boost::openmethod::initialize();
    Variable a{2}, b{3}, c{4};
    Plus d{a, b};
    Times e{d, c};
    std::cout << value(e) << "\n"; // 20
}

// tag::call_value[]
int call_value(const Node& node) {
    return value(node);
}
// end::call_value[]

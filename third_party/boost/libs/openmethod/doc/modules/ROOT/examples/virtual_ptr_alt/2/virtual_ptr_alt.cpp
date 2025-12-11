// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <boost/openmethod.hpp>

// tag::content[]
struct Node {
    boost::openmethod::vptr_type vptr;
    using registry = boost::openmethod::default_registry; // short alias

    friend auto boost_openmethod_vptr(const Node& node, registry*) {
        return node.vptr;
    }

    Node() {
        vptr = registry::static_vptr<Node>;
    }
};

struct Variable : Node {
    Variable(int value) : v(value) {
        vptr = registry::static_vptr<Variable>;
    }

    int v;
};

struct Plus : Node {
    Plus(const Node& left, const Node& right) : left(left), right(right) {
        vptr = registry::static_vptr<Plus>;
    }

    const Node& left;
    const Node& right;
};

struct Times : Node {
    Times(const Node& left, const Node& right) : left(left), right(right) {
        vptr = registry::static_vptr<Times>;
    }

    const Node& left;
    const Node& right;
};

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

BOOST_OPENMETHOD_CLASSES(Node, Variable, Plus, Times);

auto main() -> int {
    boost::openmethod::initialize();
    Variable a{2}, b{3}, c{4};
    Plus d{a, b};
    Times e{d, c};
    std::cout << value(e) << "\n"; // 20
}
// end::content[]

// tag::call_value[]
int call_value(const Node& node) {
    return value(node);
}
// end::call_value[]

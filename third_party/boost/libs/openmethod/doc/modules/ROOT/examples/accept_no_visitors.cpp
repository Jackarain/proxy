// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <memory>
#include <string>

#include <boost/openmethod.hpp>
#include <boost/openmethod/interop/std_shared_ptr.hpp>
#include <boost/openmethod/initialize.hpp>

using namespace boost::openmethod::aliases;

using std::cout;
using std::string;

struct Node {
    virtual ~Node() {
    }
};

struct Plus : Node {
    Plus(
        shared_virtual_ptr<const Node> left,
        shared_virtual_ptr<const Node> right)
        : left(left), right(right) {
    }

    shared_virtual_ptr<const Node> left, right;
};

struct Times : Node {
    Times(
        shared_virtual_ptr<const Node> left,
        shared_virtual_ptr<const Node> right)
        : left(left), right(right) {
    }

    shared_virtual_ptr<const Node> left, right;
};

struct Variable : Node {
    explicit Variable(int value) : value(value) {
    }
    int value;
};

// =============================================================================
// add behavior to existing classes, without changing them

BOOST_OPENMETHOD_CLASSES(Node, Plus, Times, Variable);

// -----------------------------------------------------------------------------
// evaluate

BOOST_OPENMETHOD(value, (virtual_ptr<const Node>), int);

BOOST_OPENMETHOD_OVERRIDE(value, (virtual_ptr<const Plus> node), int) {
    return value(node->left) + value(node->right);
}

BOOST_OPENMETHOD_OVERRIDE(value, (virtual_ptr<const Times> node), int) {
    return value(node->left) * value(node->right);
}

BOOST_OPENMETHOD_OVERRIDE(value, (virtual_ptr<const Variable> node), int) {
    return node->value;
}

// -----------------------------------------------------------------------------
// render as Forth

BOOST_OPENMETHOD(as_forth, (virtual_ptr<const Node>), string);

BOOST_OPENMETHOD_OVERRIDE(as_forth, (virtual_ptr<const Plus> node), string) {
    return as_forth(node->left) + " " + as_forth(node->right) + " +";
}

BOOST_OPENMETHOD_OVERRIDE(as_forth, (virtual_ptr<const Times> node), string) {
    return as_forth(node->left) + " " + as_forth(node->right) + " *";
}

BOOST_OPENMETHOD_OVERRIDE(as_forth, (virtual_ptr<const Variable> node), string) {
    return std::to_string(node->value);
}

// -----------------------------------------------------------------------------
// render as Lisp

BOOST_OPENMETHOD(as_lisp, (virtual_ptr<const Node>), string);

BOOST_OPENMETHOD_OVERRIDE(as_lisp, (virtual_ptr<const Plus> node), string) {
    return "(plus " + as_lisp(node->left) + " " + as_lisp(node->right) + ")";
}

BOOST_OPENMETHOD_OVERRIDE(as_lisp, (virtual_ptr<const Times> node), string) {
    return "(times " + as_lisp(node->left) + " " + as_lisp(node->right) + ")";
}

BOOST_OPENMETHOD_OVERRIDE(as_lisp, (virtual_ptr<const Variable> node), string) {
    return std::to_string(node->value);
}

// -----------------------------------------------------------------------------

auto main() -> int {
    boost::openmethod::initialize();

    shared_virtual_ptr<Node> node = make_shared_virtual<Times>(
        make_shared_virtual<Variable>(2),
        make_shared_virtual<Plus>(
            make_shared_virtual<Variable>(3), make_shared_virtual<Variable>(4)));

    cout << as_forth(node) << " = " << as_lisp(node) << " = " << value(node)
         << "\n";
    // output:
    // 2 3 4 + * = (times 2 (plus 3 4)) = 14

    return 0;
}

// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// clang-format off

// tag::classes[]
struct Node {
    virtual ~Node() {}
    virtual int value() const = 0;
    // our custom RTTI:
    Node(unsigned type) : type(type) {}
    unsigned type;
    static constexpr unsigned static_type = 1;
};

struct Variable : Node {
    static constexpr unsigned static_type = 2;
    Variable(int value) : Node(static_type), v(value) {}
    int value() const override { return v; }
    int v;
};

struct Plus : Node {
    static constexpr unsigned static_type = 3;
    Plus(const Node& left, const Node& right)
        : Node(static_type), left(left), right(right) {}
    int value() const override { return left.value() + right.value(); }
    const Node& left; const Node& right;
};

struct Times : Node {
    static constexpr unsigned static_type = 4;
    Times(const Node& left, const Node& right)
        : Node(static_type), left(left), right(right) {}
    int value() const override { return left.value() * right.value(); }
    const Node& left; const Node& right;
};
// end::classes[]

// tag::policy[]
#include <boost/openmethod/preamble.hpp>
#include <boost/openmethod/policies/vptr_vector.hpp>

struct custom_rtti : boost::openmethod::policies::rtti {
    template<class Registry>
    struct fn : defaults {
        template<class T>
        static constexpr bool is_polymorphic = std::is_base_of_v<Node, T>;

        using type_id = boost::openmethod::type_id; // for brevity

        template<typename T>
        static auto static_type() {
            if constexpr (is_polymorphic<T>) {
                return reinterpret_cast<type_id>(T::static_type);
            } else {
                return reinterpret_cast<type_id>(0);
            }
        }

        template<typename T>
        static auto dynamic_type(const T& obj) {
            if constexpr (is_polymorphic<T>) {
                return reinterpret_cast<type_id>(obj.type);
            } else {
                return reinterpret_cast<type_id>(0);
            }
        }
    };
};
// end::policy[]

// tag::registry[]
struct custom_registry : boost::openmethod::registry<
        custom_rtti, boost::openmethod::policies::vptr_vector> {};

#define BOOST_OPENMETHOD_DEFAULT_REGISTRY custom_registry
// end::registry[]

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>
#include <iostream>

using boost::openmethod::virtual_ptr;

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

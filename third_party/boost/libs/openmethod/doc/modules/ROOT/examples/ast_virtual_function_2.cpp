// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// clang-format off

// tag::all[]
#include <iostream>

struct Node {
    virtual ~Node() {}
    virtual int value() const = 0;
    virtual void postfix(std::ostream& os) const = 0;
};

struct Variable : Node {
    Variable(int value) : v(value) {}
    int value() const override { return v; }
    virtual void postfix(std::ostream& os) const override { os << v; }
    int v;
};

struct Plus : Node {
    Plus(const Node& left, const Node& right) : left(left), right(right) {}
    int value() const override { return left.value() + right.value(); }
    void postfix(std::ostream& os) const override {
        left.postfix(os); os << ' '; right.postfix(os); os << " +";
    }
    const Node& left; const Node& right;
};

struct Times : Node {
    Times(const Node& left, const Node& right) : left(left), right(right) {}
    int value() const override { return left.value() * right.value(); }
    void postfix(std::ostream& os) const override {
        left.postfix(os); os << ' '; right.postfix(os); os << " *";
    }
    const Node& left; const Node& right;
};

int main() {
    Variable a{2}, b{3}, c{4};
    Plus d{a, b}; Times e{d, c};
    e.postfix(std::cout);
    std::cout << " = " << e.value() << "\n"; // 2 3 + 4 * = 20
}
// end::all[]

void call_virtual_function(const Node& node, std::ostream& os) {
    node.postfix(os);
}

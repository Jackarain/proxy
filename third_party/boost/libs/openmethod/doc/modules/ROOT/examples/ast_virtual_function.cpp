// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// clang-format off

// tag::content[]
#include <iostream>

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
    Plus(const Node& left, const Node& right) : left(left), right(right) {}
    int value() const override { return left.value() + right.value(); }
    const Node& left; const Node& right;
};

struct Times : Node {
    Times(const Node& left, const Node& right) : left(left), right(right) {}
    int value() const override { return left.value() * right.value(); }
    const Node& left; const Node& right;
};

int main() {
    Variable a{2}, b{3}, c{4};
    Plus d{a, b}; Times e{d, c};
    std::cout << e.value() << "\n"; // 20
}
// end::content[]

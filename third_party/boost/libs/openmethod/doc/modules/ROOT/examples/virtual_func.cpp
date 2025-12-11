// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// clang-format off

// tag::code[]

#include <iostream>
#include <memory>

struct Animal {
    Animal(std::string name) : name(name) {}
    virtual ~Animal() = default;
    virtual void poke(std::ostream&) = 0;
    std::string name;
};

struct Cat : Animal {
    using Animal::Animal;

    void poke(std::ostream& os) override {
        os << name << " hisses";
    }
};

struct Dog : Animal {
    using Animal::Animal;

    void poke(std::ostream& os) override {
        os << name << " barks";
    }
};

struct Bulldog : Dog {
    using Dog::Dog;

    void poke(std::ostream& os) override {
        Dog::poke(os);
        os << " and bites back";
    }
};

auto main() -> int {
    std::unique_ptr<Animal> a(new Cat("Felix"));
    std::unique_ptr<Animal> b(new Dog("Snoopy"));
    std::unique_ptr<Animal> c(new Bulldog("Hector"));

    a->poke(std::cout); // prints "Felix hisses"
    std::cout << ".\n";

    b->poke(std::cout); // prints "Snoopy barks"
    std::cout << ".\n";

    c->poke(std::cout); // prints "Hector barks and bites back"
    std::cout << ".\n";

    return 0;
}

// end::code[]

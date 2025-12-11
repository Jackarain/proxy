// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <string>

#include <boost/openmethod/default_registry.hpp>

namespace bom = boost::openmethod;
struct test_registry : bom::default_registry::without<
                           bom::policies::vptr, bom::policies::type_hash> {};

#define BOOST_OPENMETHOD_DEFAULT_REGISTRY test_registry

#include <boost/openmethod.hpp>
#include <boost/openmethod/inplace_vptr.hpp>
#include <boost/openmethod/interop/std_shared_ptr.hpp>
#include <boost/openmethod/initialize.hpp>

#define BOOST_TEST_MODULE intrusive
#include <boost/test/unit_test.hpp>

namespace bom = boost::openmethod;
using bom::virtual_;

struct Animal : bom::inplace_vptr_base<Animal> {
    explicit Animal(std::ostream& os);
    ~Animal();
    std::ostream& os;
};

struct Cat : Animal, bom::inplace_vptr_derived<Cat, Animal> {
    explicit Cat(std::ostream& os);
    ~Cat();
};

struct Pet : bom::inplace_vptr_base<Pet> {
    explicit Pet(std::ostream& os);
    ~Pet();
    std::string name;
    std::ostream& os;
};

struct DomesticCat : Cat,
                     Pet,
                     bom::inplace_vptr_derived<DomesticCat, Cat, Pet> {
    explicit DomesticCat(std::ostream& os);
    ~DomesticCat();
};

BOOST_OPENMETHOD(
    speak, (virtual_<const Animal&> animal, std::ostream& os), void);

BOOST_OPENMETHOD(describe, (virtual_<const Pet&> pet, std::ostream& os), void);

BOOST_OPENMETHOD(
    meet,
    (virtual_<std::shared_ptr<Animal>>,
     virtual_<const std::shared_ptr<Animal>&>, std::ostream& os),
    void);

BOOST_OPENMETHOD_OVERRIDE(
    meet,
    (std::shared_ptr<Animal>, const std::shared_ptr<Animal>&, std::ostream& os),
    void) {
    os << "ignore\n";
}

Animal::Animal(std::ostream& os) : os(os) {
    speak(*this, os);
}

Animal::~Animal() {
    speak(*this, os);
}

Cat::Cat(std::ostream& os) : Animal(os) {
    speak(*this, os);
}

Cat::~Cat() {
    speak(*this, os);
}

Pet::Pet(std::ostream& os) : os(os) {
    describe(*this, os);
}

Pet::~Pet() {
    describe(*this, os);
}

DomesticCat::DomesticCat(std::ostream& os) : Cat(os), Pet(os) {
    name = "Felix";
    describe(*this, os);
}

DomesticCat::~DomesticCat() {
    describe(*this, Cat::os);
}

BOOST_OPENMETHOD_OVERRIDE(speak, (const Animal&, std::ostream& os), void) {
    os << "???\n";
}

BOOST_OPENMETHOD_OVERRIDE(speak, (const Cat&, std::ostream& os), void) {
    os << "meow\n";
}

BOOST_OPENMETHOD_OVERRIDE(describe, (const Pet&, std::ostream& os), void) {
    os << "I am a pet\n";
}

BOOST_OPENMETHOD_OVERRIDE(
    describe, (const DomesticCat& pet, std::ostream& os), void) {
    os << "I am " << pet.name << " the cat\n";
}

// Check that we pick one of the vptrs in presence of MI, dodging ambiguity
// issues.
BOOST_OPENMETHOD(
    cat_influencer, (virtual_<const DomesticCat&> cat, std::ostream& os), void);

BOOST_OPENMETHOD_OVERRIDE(
    cat_influencer, (const DomesticCat& cat, std::ostream& os), void) {
    os << "Follow " << cat.name << " the cat on YouTube\n";
}

BOOST_AUTO_TEST_CASE(intrusive_mode) {
    bom::initialize();

    std::shared_ptr<DomesticCat> cat;
    std::ostringstream cd_output;

    {
        std::ostringstream output;
        cat = std::make_shared<DomesticCat>(cd_output);

        BOOST_TEST(
            cd_output.str() ==
            "???\n"
            "meow\n"
            "I am a pet\n"
            "I am Felix the cat\n");
    }

    {
        std::ostringstream output;
        describe(*cat, output);
        BOOST_TEST(output.str() == "I am Felix the cat\n");
    }

    {
        std::ostringstream output;
        cat_influencer(*cat, output);
        BOOST_TEST(output.str() == "Follow Felix the cat on YouTube\n");
    }

    {
        cd_output.str("");
        cat.reset();

        BOOST_TEST(
            cd_output.str() ==
            "I am Felix the cat\n"
            "I am a pet\n"
            "meow\n"
            "???\n");
    }

    {
        auto felix = std::make_shared<Cat>(cd_output),
             sylvester = std::make_shared<Cat>(cd_output);

        std::ostringstream output;
        meet(felix, sylvester, output);

        BOOST_TEST(output.str() == "ignore\n");
    }
}

struct indirect_policy : test_registry::with<bom::policies::indirect_vptr> {};

struct Indirect : bom::inplace_vptr_base<Indirect, indirect_policy> {};

BOOST_OPENMETHOD(whatever, (virtual_<Indirect&>), void, indirect_policy);

BOOST_OPENMETHOD_OVERRIDE(whatever, (Indirect&), void) {
}

BOOST_AUTO_TEST_CASE(core_intrusive_vptr) {
    bom::initialize<indirect_policy>();
    Indirect i;
    BOOST_TEST(
        boost_openmethod_vptr(i, nullptr) ==
        indirect_policy::static_vptr<Indirect>);
    indirect_policy::static_vptr<Indirect> = nullptr;
    BOOST_TEST(boost_openmethod_vptr(i, nullptr) == nullptr);
}

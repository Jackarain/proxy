// qright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or q at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/openmethod.hpp>
#include <boost/openmethod/interop/std_shared_ptr.hpp>
#include <boost/openmethod/initialize.hpp>

#define BOOST_TEST_MODULE openmethod
#include <boost/test/unit_test.hpp>

using namespace boost::openmethod;

namespace polymorphic {

struct Animal {
    virtual ~Animal() {
    }
};
struct Dog : Animal {};
BOOST_OPENMETHOD_CLASSES(Animal, Dog);
BOOST_OPENMETHOD(poke, (virtual_ptr<Animal>), void);

void instiantiate_poke(virtual_ptr<Dog> snoopy) {
    poke(snoopy);
}

BOOST_AUTO_TEST_CASE(virtual_ptr_examples_polymorphic) {
    {
        initialize(trace());

        {
            virtual_ptr<Dog> p{nullptr};

            BOOST_TEST(p.get() == nullptr);
            BOOST_TEST(p.vptr() == nullptr);
        }

        {
            Dog snoopy;
            Animal& animal = snoopy;

            virtual_ptr<Animal> p = animal;

            BOOST_TEST(p.get() == &snoopy);
            BOOST_TEST(p.vptr() == default_registry::static_vptr<Dog>);
        }

        {
            Dog snoopy;
            Animal* animal = &snoopy;

            virtual_ptr<Animal> p = animal;

            BOOST_TEST(p.get() == &snoopy);
            BOOST_TEST(p.vptr() == default_registry::static_vptr<Dog>);
        }

        {
            virtual_ptr<Animal> p{nullptr};
            Dog snoopy;
            Animal* animal = &snoopy;

            p = animal;

            BOOST_TEST(p.get() == &snoopy);
            BOOST_TEST(p.vptr() == default_registry::static_vptr<Dog>);
        }

        {
            Dog snoopy;
            virtual_ptr<Animal> p = final_virtual_ptr(snoopy);

            p = nullptr;

            BOOST_TEST(p.get() == nullptr);
            BOOST_TEST(p.vptr() == nullptr);
        }
    }
}

BOOST_AUTO_TEST_CASE(smart_virtual_ptr_examples) {
    initialize();

    {
        virtual_ptr<std::shared_ptr<Dog>> p;
        BOOST_TEST(p.get() == nullptr);
        BOOST_TEST(p.vptr() == nullptr);
    }

    {
        virtual_ptr<std::shared_ptr<Dog>> p{nullptr};
        BOOST_TEST(p.get() == nullptr);
        BOOST_TEST(p.vptr() == nullptr);
    }

    {
        const std::shared_ptr<Dog> snoopy = std::make_shared<Dog>();
        virtual_ptr<std::shared_ptr<Animal>> p = snoopy;

        BOOST_TEST(p.get() == snoopy.get());
        BOOST_TEST(p.vptr() == default_registry::static_vptr<Dog>);
    }

    {
        std::shared_ptr<Dog> snoopy = std::make_shared<Dog>();
        virtual_ptr<std::shared_ptr<Animal>> p = snoopy;

        BOOST_TEST(p.get() == snoopy.get());
        BOOST_TEST(p.vptr() == default_registry::static_vptr<Dog>);
    }

    {
        std::shared_ptr<Dog> snoopy = std::make_shared<Dog>();
        Dog* moving = snoopy.get();

        virtual_ptr<std::shared_ptr<Animal>> p = std::move(snoopy);

        BOOST_TEST(p.get() == moving);
        BOOST_TEST(p.vptr() == default_registry::static_vptr<Dog>);
        BOOST_TEST(snoopy.get() == nullptr);
    }
    {
        const virtual_ptr<std::shared_ptr<Dog>> snoopy =
            make_shared_virtual<Dog>();
        virtual_ptr<std::shared_ptr<Animal>> p = std::move(snoopy);

        BOOST_TEST(snoopy.get() != nullptr);
        BOOST_TEST(p.get() == snoopy.get());
        BOOST_TEST(p.vptr() == default_registry::static_vptr<Dog>);
    }

    {
        virtual_ptr<std::shared_ptr<Dog>> snoopy = make_shared_virtual<Dog>();
        Dog* moving = snoopy.get();

        virtual_ptr<std::shared_ptr<Animal>> p = std::move(snoopy);

        BOOST_TEST(p.get() == moving);
        BOOST_TEST(p.vptr() == default_registry::static_vptr<Dog>);
        BOOST_TEST(snoopy.get() == nullptr);
        BOOST_TEST(snoopy.vptr() == nullptr);
    }

    {
        virtual_ptr<std::shared_ptr<Dog>> p = make_shared_virtual<Dog>();

        p = nullptr;

        BOOST_TEST(p.get() == nullptr);
        BOOST_TEST(p.vptr() == nullptr);
        BOOST_TEST((p == virtual_ptr<std::shared_ptr<Dog>>()));
    }

    {
        const virtual_ptr<std::shared_ptr<Dog>> snoopy =
            make_shared_virtual<Dog>();
        virtual_ptr<std::shared_ptr<Dog>> p;

        p = snoopy;

        BOOST_TEST(p.get() != nullptr);
        BOOST_TEST(p.get() == snoopy.get());
        BOOST_TEST(p.vptr() == default_registry::static_vptr<Dog>);
        BOOST_TEST(snoopy.vptr() == default_registry::static_vptr<Dog>);
    }

    {
        virtual_ptr<std::shared_ptr<Dog>> snoopy = make_shared_virtual<Dog>();
        Dog* moving = snoopy.get();
        virtual_ptr<std::shared_ptr<Dog>> p;

        p = std::move(snoopy);

        BOOST_TEST(p.get() == moving);
        BOOST_TEST(p.vptr() == default_registry::static_vptr<Dog>);
        BOOST_TEST(snoopy.get() == nullptr);
        BOOST_TEST(snoopy.vptr() == nullptr);
    }
}
} // namespace polymorphic

namespace non_polymorphic {

struct Animal {};       // polymorphic not required
struct Dog : Animal {}; // polymorphic not required
BOOST_OPENMETHOD_CLASSES(Animal, Dog);

// codecov:ignore:start
BOOST_OPENMETHOD(poke, (virtual_ptr<Animal>), void);

void instantiate_poke(virtual_ptr<Dog> snoopy) {
    poke(snoopy);
}
// codecov:ignore:end

BOOST_AUTO_TEST_CASE(virtual_ptr_examples_non_polymorphic) {
    {
        initialize();

        {
            Dog snoopy;
            virtual_ptr<Dog> dog = final_virtual_ptr(snoopy);

            virtual_ptr<Animal> p(dog);

            BOOST_TEST(p.get() == &snoopy);
            BOOST_TEST(p.vptr() == default_registry::static_vptr<Dog>);
        }

        {
            Dog snoopy;
            virtual_ptr<Dog> dog = final_virtual_ptr(snoopy);
            virtual_ptr<Animal> p{nullptr};

            p = dog;

            BOOST_TEST(p.get() == &snoopy);
            BOOST_TEST(p.vptr() == default_registry::static_vptr<Dog>);
        }

        {
            virtual_ptr<std::shared_ptr<Animal>> snoopy =
                make_shared_virtual<Dog>();
            virtual_ptr<Animal> p = snoopy;

            BOOST_TEST(p.get() == snoopy.get());
            BOOST_TEST(p.vptr() == default_registry::static_vptr<Dog>);
        }

        static_assert(
            std::is_constructible_v<
                shared_virtual_ptr<Animal>, virtual_ptr<Dog>> == false);

        {
            virtual_ptr<std::shared_ptr<Animal>> snoopy =
                make_shared_virtual<Dog>();
            virtual_ptr<Animal> p;

            p = snoopy;

            BOOST_TEST(p.get() == snoopy.get());
            BOOST_TEST(p.vptr() == default_registry::static_vptr<Dog>);
        }

        static_assert(
            std::is_assignable_v<
                shared_virtual_ptr<Animal>&, virtual_ptr<Dog>> == false);
    }
}
} // namespace non_polymorphic

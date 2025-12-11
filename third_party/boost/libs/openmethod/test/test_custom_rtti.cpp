// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MODULE openmethod
#include <boost/test/unit_test.hpp>
#include <boost/utility/identity_type.hpp>

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>

#include "test_util.hpp"

using namespace boost::openmethod;

namespace type_hash {

struct Animal {
    const char* name;

    Animal(const char* name, const char* type) : name(name), type(type) {
    }

    static constexpr const char* static_type = "Animal";
    const char* type;
};

struct Dog : Animal {
    Dog(const char* name, const char* type = static_type) : Animal(name, type) {
    }

    static constexpr const char* static_type = "Dog";
};

struct Cat : Animal {
    Cat(const char* name, const char* type = static_type) : Animal(name, type) {
    }

    static constexpr const char* static_type = "Cat";
};

struct custom_rtti : policies::rtti {
    template<class Registry>
    struct fn : defaults {
        template<class T>
        static constexpr bool is_polymorphic = std::is_base_of_v<Animal, T>;

        template<typename T>
        static auto static_type() {
            if constexpr (is_polymorphic<T>) {
                return T::static_type;
            } else {
                return nullptr;
            }
        }

        template<typename T>
        static auto dynamic_type(const T& obj) {
            if constexpr (is_polymorphic<T>) {
                return obj.type;
            } else {
                return nullptr;
            }
        }

        template<class Stream>
        static void type_name(type_id type, Stream& stream) {
            stream
                << (type == nullptr ? "?"
                                    : reinterpret_cast<const char*>(type));
        }

        static auto type_index(type_id type) {
            return std::string_view(
                (type == nullptr ? "?" : reinterpret_cast<const char*>(type)));
        }
    };
};

struct test_registry : test_registry_<__COUNTER__>::with<custom_rtti> {};

BOOST_OPENMETHOD_CLASSES(Animal, Dog, Cat, test_registry);

BOOST_OPENMETHOD(poke, (virtual_<Animal&>, std::ostream&), void, test_registry);

BOOST_OPENMETHOD_OVERRIDE(poke, (Dog & dog, std::ostream& os), void) {
    os << dog.name << " barks.";
}

BOOST_OPENMETHOD_OVERRIDE(poke, (Cat & cat, std::ostream& os), void) {
    os << cat.name << " hisses.";
}

BOOST_AUTO_TEST_CASE(custom_rtti_simple_projection) {
    initialize<test_registry>();

    Animal &&a = Dog("Snoopy"), &&b = Cat("Sylvester");

    {
        std::stringstream os;
        poke(a, os);
        BOOST_TEST(os.str() == "Snoopy barks.");
    }
    {
        std::stringstream os;
        poke(b, os);
        BOOST_TEST(os.str() == "Sylvester hisses.");
    }
}
} // namespace type_hash

namespace no_projection {

struct Animal {
    const char* name;

    Animal(const char* name, std::size_t type) : name(name), type(type) {
    }

    static constexpr std::size_t static_type = 0;
    std::size_t type;
};

struct Dog : Animal {
    Dog(const char* name, std::size_t type = static_type) : Animal(name, type) {
    }

    static constexpr std::size_t static_type = 1;
};

struct Cat : Animal {
    Cat(const char* name, std::size_t type = static_type) : Animal(name, type) {
    }

    static constexpr std::size_t static_type = 2;
};

struct custom_rtti : policies::rtti {
    template<class Registry>
    struct fn : defaults {
        template<class T>
        static constexpr bool is_polymorphic = std::is_base_of_v<Animal, T>;

        inline static auto invalid_type_id = type_id(0xFFFFFF);

        template<typename T>
        static auto static_type() {
            if constexpr (is_polymorphic<T>) {
                return type_id(T::static_type);
            } else {
                return invalid_type_id;
            }
        }

        template<typename T>
        static auto dynamic_type(const T& obj) {
            if constexpr (is_polymorphic<T>) {
                return type_id(obj.type);
            } else {
                return invalid_type_id;
            }
        }

        template<class Stream>
        static void type_name(type_id type, Stream& stream) {
            static const char* name[] = {"Animal", "Dog", "Cat"};
            stream << (type == invalid_type_id ? "?" : name[std::size_t(type)]);
        }

        static auto type_index(type_id type) {
            return type;
        }
    };
};

struct test_registry : test_registry_<__COUNTER__>::with<custom_rtti>::without<
                           policies::type_hash> {};

BOOST_OPENMETHOD_CLASSES(Animal, Dog, Cat, test_registry);

BOOST_OPENMETHOD(poke, (virtual_<Animal&>, std::ostream&), void, test_registry);

BOOST_OPENMETHOD_OVERRIDE(poke, (Dog & dog, std::ostream& os), void) {
    os << dog.name << " barks.";
}

BOOST_OPENMETHOD_OVERRIDE(poke, (Cat & cat, std::ostream& os), void) {
    os << cat.name << " hisses.";
}

void call_poke(Animal& a, std::ostream& os) {
    return poke(a, os);
}
// mov     rax, qword ptr [rdi + 8]
// mov     rcx, qword ptr [rip + domain<test_registry>::context+24]
// mov     rax, qword ptr [rcx + 8*rax]
// mov     rcx, qword ptr [rip + method<test_registry, poke, void
// (virtual_<Animal&>, basic_ostream<char, char_traits<char> >&)>::fn+80] mov
// rax, qword ptr [rax + 8*rcx] jmp     rax                             #
// TAILCALL

BOOST_AUTO_TEST_CASE(custom_rtti_simple) {
    BOOST_TEST(Animal::static_type == 0u);
    BOOST_TEST(Dog::static_type == 1u);
    BOOST_TEST(Cat::static_type == 2u);
    initialize<test_registry>();

    Animal &&a = Dog("Snoopy"), &&b = Cat("Sylvester");

    {
        std::stringstream os;
        poke(a, os);
        BOOST_TEST(os.str() == "Snoopy barks.");
    }
    {
        std::stringstream os;
        poke(b, os);
        BOOST_TEST(os.str() == "Sylvester hisses.");
    }
}

namespace using_vptr {

template<class C>
using vptr = virtual_ptr<C, test_registry>;

BOOST_OPENMETHOD(poke, (vptr<Animal>, std::ostream&), void, test_registry);

BOOST_OPENMETHOD_OVERRIDE(poke, (vptr<Dog> dog, std::ostream& os), void) {
    os << dog->name << " barks.";
}

BOOST_OPENMETHOD_OVERRIDE(poke, (vptr<Cat> cat, std::ostream& os), void) {
    os << cat->name << " hisses.";
}

void call_poke(vptr<Animal> a, std::ostream& os) {
    // mov     rax, qword ptr [rip + method<test_registry, poke, ...>::fn+80]
    // mov     rax, qword ptr [rsi + 8*rax]
    // jmp     rax                             # TAILCALL
    return poke(a, os);
}

} // namespace using_vptr
} // namespace no_projection

namespace virtual_base {

struct Animal {
    const char* name;

    Animal(const char* name, std::size_t type) : name(name), type(type) {
    }

    virtual auto cast_aux(std::size_t to_type) -> void* {
        return to_type == static_type ? this : nullptr;
    }

    static constexpr std::size_t static_type = 0;
    std::size_t type;
};

template<typename Derived, typename Base>
auto custom_dynamic_cast(Base& obj) -> Derived {
    using derived_type = std::remove_cv_t<std::remove_reference_t<Derived>>;
    return *reinterpret_cast<derived_type*>(
        const_cast<std::remove_cv_t<Base>&>(obj).cast_aux(
            derived_type::static_type));
}

struct Dog : virtual Animal {
    Dog(const char* name, std::size_t type = static_type) : Animal(name, type) {
    }

    auto cast_aux(std::size_t to_type) -> void* override {
        return to_type == static_type ? this : Animal::cast_aux(to_type);
    }

    static constexpr std::size_t static_type = 1;
};

struct Cat : virtual Animal {
    Cat(const char* name, std::size_t type = static_type) : Animal(name, type) {
    }

    auto cast_aux(std::size_t to_type) -> void* override {
        return to_type == static_type ? this : Animal::cast_aux(to_type);
    }

    static constexpr std::size_t static_type = 2;
};

struct custom_rtti : policies::rtti {
    template<class Registry>
    struct fn : defaults {
        template<class T>
        static constexpr bool is_polymorphic = std::is_base_of_v<Animal, T>;

        template<typename T>
        static auto static_type() {
            if constexpr (is_polymorphic<T>) {
                return type_id(T::static_type);
            } else {
                return type_id(0xFFFF);
            }
        }

        template<typename T>
        static auto dynamic_type(const T& obj) {
            if constexpr (is_polymorphic<T>) {
                return type_id(obj.type);
            } else {
                return type_id(0xFFFF);
            }
        }

        template<class Stream>
        static void type_name(type_id type, Stream& stream) {
            static const char* name[] = {"Animal", "Dog", "Cat"};
            stream << (type == type_id(0xFFFF) ? "?" : name[std::size_t(type)]);
        }

        static auto type_index(type_id type) {
            return type;
        }

        template<typename Derived, typename Base>
        static auto dynamic_cast_ref(Base&& obj) -> Derived {
            return custom_dynamic_cast<Derived>(obj);
        }
    };
};

struct test_registry : test_registry_<__COUNTER__>::with<custom_rtti>::without<
                           policies::type_hash> {};

BOOST_OPENMETHOD_CLASSES(Animal, Dog, Cat, test_registry);

BOOST_OPENMETHOD(poke, (virtual_<Animal&>, std::ostream&), void, test_registry);

BOOST_OPENMETHOD_OVERRIDE(poke, (Dog & dog, std::ostream& os), void) {
    os << dog.name << " barks.";
}

BOOST_OPENMETHOD_OVERRIDE(poke, (Cat & cat, std::ostream& os), void) {
    os << cat.name << " hisses.";
}

void call_poke(Animal& a, std::ostream& os) {
    return poke(a, os);
}
// mov     rax, qword ptr [rdi + 8]
// mov     rcx, qword ptr [rip + domain<test_registry>::context+24]
// mov     rax, qword ptr [rcx + 8*rax]
// mov     rcx, qword ptr [rip + method<test_registry, poke, void
// (virtual_<Animal&>, basic_ostream<char, char_traits<char> >&)>::fn+80] mov
// rax, qword ptr [rax + 8*rcx] jmp     rax                             #
// TAILCALL

BOOST_AUTO_TEST_CASE(virtual_base) {
    BOOST_TEST(Animal::static_type == 0u);
    BOOST_TEST(Dog::static_type == 1u);
    BOOST_TEST(Cat::static_type == 2u);
    initialize<test_registry>();

    Animal &&a = Dog("Snoopy"), &&b = Cat("Sylvester");

    {
        std::stringstream os;
        poke(a, os);
        BOOST_TEST(os.str() == "Snoopy barks.");
    }
    {
        std::stringstream os;
        poke(b, os);
        BOOST_TEST(os.str() == "Sylvester hisses.");
    }
}

namespace using_vptr {

template<class C>
using vptr = virtual_ptr<C, test_registry>;

BOOST_OPENMETHOD(poke, (vptr<Animal>, std::ostream&), void, test_registry);

BOOST_OPENMETHOD_OVERRIDE(poke, (vptr<Dog> dog, std::ostream& os), void) {
    os << dog->name << " barks.";
}

BOOST_OPENMETHOD_OVERRIDE(poke, (vptr<Cat> cat, std::ostream& os), void) {
    os << cat->name << " hisses.";
}

void call_poke(vptr<Animal> a, std::ostream& os) {
    // mov     rax, qword ptr [rip + method<test_registry, poke, ...>::fn+80]
    // mov     rax, qword ptr [rsi + 8*rax]
    // jmp     rax                             # TAILCALL
    return poke(a, os);
}

} // namespace using_vptr
} // namespace virtual_base

namespace deferred_type_id {

struct Animal {
    const char* name;

    Animal(const char* name, std::size_t type) : name(name), type(type) {
    }

    static std::size_t last_type_id;
    static std::size_t static_type;
    std::size_t type;
};

std::size_t Animal::last_type_id;
std::size_t Animal::static_type = ++Animal::last_type_id;

struct Dog : Animal {
    Dog(const char* name, std::size_t type = static_type) : Animal(name, type) {
    }

    static std::size_t static_type;
};

std::size_t Dog::static_type = ++Animal::last_type_id;

struct Cat : Animal {
    Cat(const char* name, std::size_t type = static_type) : Animal(name, type) {
    }

    static std::size_t static_type;
};

std::size_t Cat::static_type = ++Animal::last_type_id;

struct custom_rtti : policies::deferred_static_rtti {
    template<class Registry>
    struct fn : defaults {
        template<class T>
        static constexpr bool is_polymorphic = std::is_base_of_v<Animal, T>;

        template<typename T>
        static auto static_type() {
            if constexpr (is_polymorphic<T>) {
                return type_id(T::static_type);
            } else {
                return type_id(0);
            }
        }

        template<typename T>
        static auto dynamic_type(const T& obj) {
            if constexpr (is_polymorphic<T>) {
                return type_id(obj.type);
            } else {
                return type_id(0);
            }
        }

        template<class Stream>
        static void type_name(type_id type, Stream& stream) {
            static const char* name[] = {"?", "Animal", "Dog", "Cat"};
            stream << name[std::size_t(type)];
        }

        static auto type_index(type_id type) {
            return type;
        }
    };
};

struct test_registry : test_registry_<__COUNTER__>::with<custom_rtti>::without<
                           policies::type_hash> {};

BOOST_OPENMETHOD_CLASSES(Animal, Dog, Cat, test_registry);

BOOST_OPENMETHOD(poke, (virtual_<Animal&>, std::ostream&), void, test_registry);

BOOST_OPENMETHOD_OVERRIDE(poke, (Dog & dog, std::ostream& os), void) {
    os << dog.name << " barks.";
}

BOOST_OPENMETHOD_OVERRIDE(poke, (Cat & cat, std::ostream& os), void) {
    os << cat.name << " hisses.";
}

BOOST_OPENMETHOD(
    meet, (virtual_<Animal&>, virtual_<Animal&>, std::ostream&), void,
    test_registry);

BOOST_OPENMETHOD_OVERRIDE(meet, (Dog&, Dog&, std::ostream& os), void) {
    os << "Both wag tails.";
}

BOOST_AUTO_TEST_CASE(custom_rtti_deferred) {
    initialize<test_registry>();

    Animal &&a = Dog("Snoopy"), &&b = Cat("Sylvester");

    {
        std::stringstream os;
        poke(a, os);
        BOOST_TEST(os.str() == "Snoopy barks.");
    }

    {
        std::stringstream os;
        poke(b, os);
        BOOST_TEST(os.str() == "Sylvester hisses.");
    }

    {
        std::stringstream os;
        meet(a, a, os);
        BOOST_TEST(os.str() == "Both wag tails.");
    }
}

} // namespace deferred_type_id

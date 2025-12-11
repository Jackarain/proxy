// felines.hpp

#ifndef FELINES_HPP
#define FELINES_HPP

#include "animal.hpp"

namespace felines {

struct Cat : animals::Animal {
    using Animal::Animal;
};

struct Cheetah : Cat {
    using Cat::Cat;
};

} // namespace felines

#endif // FELINES_HPP

# Boost.Decimal

|                  | Master                                                                                                                                                            |   Develop   |
|------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------|-------------|
| Drone            | [![Build Status](https://drone.cpp.al/api/badges/boostorg/decimal/status.svg?ref=refs/heads/master)](https://drone.cpp.al/boostorg/decimal)                 | [![Build Status](https://drone.cpp.al/api/badges/boostorg/decimal/status.svg?ref=refs/heads/develop)](https://drone.cpp.al/boostorg/decimal) |
| Github Actions   | [![CI](https://github.com/boostorg/decimal/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/boostorg/decimal/actions/workflows/ci.yml) | [![CI](https://github.com/boostorg/decimal/actions/workflows/ci.yml/badge.svg?branch=develop)](https://github.com/boostorg/decimal/actions/workflows/ci.yml)
| Codecov          | [![codecov](https://codecov.io/gh/boostorg/decimal/branch/master/graph/badge.svg?token=drvY8nnV5S)](https://codecov.io/gh/boostorg/decimal)                 | [![codecov](https://codecov.io/gh/boostorg/decimal/graph/badge.svg?token=drvY8nnV5S)](https://codecov.io/gh/boostorg/decimal) |
| Fuzzing          | [![Fuzzing](https://github.com/boostorg/decimal/actions/workflows/fuzz.yml/badge.svg?branch=master)](https://github.com/boostorg/decimal/actions/workflows/fuzz.yml) | [![Fuzzing](https://github.com/boostorg/decimal/actions/workflows/fuzz.yml/badge.svg?branch=develop)](https://github.com/boostorg/decimal/actions/workflows/fuzz.yml) |
| Metal            | [![Metal](https://github.com/boostorg/decimal/actions/workflows/metal.yml/badge.svg?branch=master)](https://github.com/boostorg/decimal/actions/workflows/metal.yml) | [![Metal](https://github.com/boostorg/decimal/actions/workflows/metal.yml/badge.svg?branch=develop)](https://github.com/boostorg/decimal/actions/workflows/metal.yml) |

---

Boost.Decimal is an implementation of [IEEE 754](https://standards.ieee.org/ieee/754/6210/) and [ISO/IEC DTR 24733](https://www.open-std.org/JTC1/SC22/WG21/docs/papers/2009/n2849.pdf) Decimal Floating Point numbers.
The library is header-only, has no dependencies, and requires C++14.

# How To Use The Library

This library is header only. It contains no other dependencies.
Simply `#include` it and use it.

## CMake

```sh
git clone https://github.com/boostorg/decimal
cd decimal
mkdir build && cd build
cmake .. OR cmake .. -DCMAKE_INSTALL_PREFIX=/your/custom/path
cmake --build . --target install
```

then you can use `find_package(boost_decimal REQUIRED)`

## vcpkg

Available in official vcpkg sources soon 

## Conan

Available in official conan sources soon

# Supported Platforms

Boost.Decimal is tested natively on Ubuntu (x86_64, s390x, and aarch64), macOS (x86_64, and Apple Silicon),
and Windows (x32 and x64); as well as emulated PPC64LE and ARM Cortex-M using QEMU with the following compilers:

* GCC 8 and later
* Clang 6 and later
* Visual Studio 2019 and later
* Intel OneAPI DPC++

# Synopsis

Decimal provides 3 IEEE-754 compliant types:

```cpp
namespace boost {
namespace decimal {

class decimal32_t;
class decimal64_t;
class decimal128_t;

} //namespace decimal
} //namespace boost
```

and also 3 similar but non-compliant types with improved runtime performance:

```cpp
namespace boost {
namespace decimal {

class decimal_fast32_t;
class decimal_fast64_t;
class decimal_fast128_t;

} //namespace decimal
} //namespace boost
```

These types operate like built-in floating point types.
They have their own implementations of the Standard-Library functions
(e.g. like those found in `<cmath>`, `<charconv>`, `<cstdlib>`, etc.).
The entire library can be conveniently included with `#include <boost/decimal.hpp>`

Using the decimal types is straightforward and can be learned by [example](https://develop.decimal.cpp.al/decimal/examples.html).
Their usage closely resembles that of built-in binary floating point types by design.

# Full Documentation

The complete documentation can be found at: https://www.boost.org/doc/libs/develop/libs/decimal/doc/html/overview.html

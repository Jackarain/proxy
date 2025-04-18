[![Boost.JSON](https://raw.githubusercontent.com/CPPAlliance/json/master/doc/images/repo-logo-3.png)](https://www.boost.org/doc/libs/release/libs/json)

Branch          | [`master`](https://github.com/boostorg/json/tree/master) | [`develop`](https://github.com/boostorg/json/tree/develop) |
--------------- | ----------------------------------------------------------- | ------------------------------------------------------------- |
[Azure](https://azure.microsoft.com/en-us/services/devops/pipelines/) | [![Build Status](https://img.shields.io/azure-devops/build/vinniefalco/2571d415-8cc8-4120-a762-c03a8eda0659/8/master)](https://vinniefalco.visualstudio.com/json/_build/latest?definitionId=5&branchName=master) | [![Build Status](https://img.shields.io/azure-devops/build/vinniefalco/2571d415-8cc8-4120-a762-c03a8eda0659/8/develop)](https://vinniefalco.visualstudio.com/json/_build/latest?definitionId=8&branchName=develop)
Docs            | [![Documentation](https://img.shields.io/badge/docs-master-brightgreen.svg)](https://www.boost.org/doc/libs/master/libs/json/) | [![Documentation](https://img.shields.io/badge/docs-develop-brightgreen.svg)](https://www.boost.org/doc/libs/develop/libs/json/)
[Drone](https://drone.io/) | [![Build Status](https://drone.cpp.al/api/badges/boostorg/json/status.svg?ref=refs/heads/master)](https://drone.cpp.al/boostorg/json) | [![Build Status](https://drone.cpp.al/api/badges/boostorg/json/status.svg?ref=refs/heads/develop)](https://drone.cpp.al/boostorg/json)
Matrix          | [![Matrix](https://img.shields.io/badge/matrix-master-brightgreen.svg)](http://www.boost.org/development/tests/master/developer/json.html) | [![Matrix](https://img.shields.io/badge/matrix-develop-brightgreen.svg)](http://www.boost.org/development/tests/develop/developer/json.html)
Fuzzing         | --- |  [![fuzz](https://github.com/boostorg/json/workflows/fuzz/badge.svg?branch=develop)](https://github.com/boostorg/json/actions?query=workflow%3Afuzz+branch%3Adevelop)
[Appveyor](https://ci.appveyor.com/) | [![Build status](https://ci.appveyor.com/api/projects/status/8csswcnmfm798203?branch=master&svg=true)](https://ci.appveyor.com/project/vinniefalco/cppalliance-json/branch/master) | [![Build status](https://ci.appveyor.com/api/projects/status/8csswcnmfm798203?branch=develop&svg=true)](https://ci.appveyor.com/project/vinniefalco/cppalliance-json/branch/develop)
[codecov.io](https://codecov.io) | [![codecov](https://codecov.io/gh/boostorg/json/branch/master/graph/badge.svg)](https://codecov.io/gh/boostorg/json/branch/master) | [![codecov](https://codecov.io/gh/boostorg/json/branch/develop/graph/badge.svg)](https://codecov.io/gh/boostorg/json/branch/develop)

# Boost.JSON

## Overview

Boost.JSON is a portable C++ library which provides containers and
algorithms that implement
[JavaScript Object Notation](https://json.org/), or simply "JSON",
a lightweight data-interchange format. This format is easy for humans to
read and write, and easy for machines to parse and generate. It is based
on a subset of the JavaScript Programming Language
([Standard ECMA-262](https://www.ecma-international.org/ecma-262/10.0/index.html)),
and is currently standardised in [RFC 8259](https://datatracker.ietf.org/doc/html/rfc8259).
JSON is a text format that is language-independent but uses conventions
that are familiar to programmers of the C-family of languages, including
C, C++, C#, Java, JavaScript, Perl, Python, and many others. These
properties make JSON an ideal data-interchange language.

This library focuses on a common and popular use-case: parsing
and serializing to and from a container called `value` which
holds JSON types. Any `value` which you build can be serialized
and then deserialized, guaranteeing that the result will be equal
to the original value. Whatever JSON output you produce with this
library will be readable by most common JSON implementations
in any language.

The `value` container is designed to be well suited as a
vocabulary type appropriate for use in public interfaces and
libraries, allowing them to be composed. The library restricts
the representable data types to the ranges which are almost
universally accepted by most JSON implementations, especially
JavaScript. The parser and serializer are both highly performant,
meeting or exceeding the benchmark performance of the best comparable
libraries. Allocators are very well supported. Code which uses these
types will be easy to understand, flexible, and performant.

Boost.JSON offers these features:

* Fast compilation
* Require only C++11
* Fast streaming parser and serializer
* Constant-time key lookup for objects
* Options to allow non-standard JSON
* Easy and safe modern API with allocator support
* Optional header-only, without linking to a library

Visit https://boost.org/libs/json for complete documentation.

## Requirements

* Requires only C++11
* Link to a built static or dynamic Boost library, or use header-only (see below)
* Supports -fno-exceptions, detected automatically

The library relies heavily on these well known C++ types in
its interfaces (henceforth termed _standard types_):

* `string_view`
* `memory_resource`, `polymorphic_allocator`
* `error_category`, `error_code`, `error_condition`, `system_error`

### Header-Only

To use as header-only; that is, to eliminate the requirement to
link a program to a static or dynamic Boost.JSON library, simply
place the following line in exactly one new or existing source
file in your project.
```
#include <boost/json/src.hpp>
```

MSVC users must also define the macro `BOOST_JSON_NO_LIB` to disable
auto-linking. Note, that if you also want to avoid linking to Boost.Container,
which is a dependency of Boost.JSON, you have to define
`BOOST_CONTAINER_NO_LIB`. In order to disable auto-linking to Boost libraries
completely you can define `BOOST_ALL_NO_LIB` instead.

### Embedded

Boost.JSON works great on embedded devices. The library uses local
stack buffers to increase the performance of some operations. On
Intel platforms these buffers are large (4KB), while on non-Intel
platforms they are small (256 bytes). To adjust the size of the
stack buffers for embedded applications define this macro when
building the library or including the function definitions:
```
#define BOOST_JSON_STACK_BUFFER_SIZE 1024
#include <boost/json/src.hpp>
```
### Endianness

Boost.JSON uses [Boost.Endian](https://www.boost.org/doc/libs/release/libs/endian/doc/html/endian.html)
in order to support both little endian and big endian platforms.

### Supported Compilers

Boost.JSON has been tested with the following compilers:

* clang: 3.5, 3.6, 3.7, 3.8, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14
* gcc: 5, 6, 7, 8, 9, 10, 11, 12
* msvc: 14.0, 14.1, 14.2, 14.3

### Supported JSON Text

The library expects input text to be encoded using UTF-8, which is a
requirement put on all JSON exchanged between systems by the
[RFC](https://datatracker.ietf.org/doc/html/rfc8259#section-8.1). Similarly,
the text generated by the library is valid UTF-8.

The RFC does not allow byte order marks (BOM) to appear in JSON text, so the
library considers BOM syntax errors.

The library supports several popular JSON extensions. These have to be
explicitly enabled.

### Visual Studio Solution

    cmake -G "Visual Studio 16 2019" -A Win32 -B bin -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/msvc.cmake
    cmake -G "Visual Studio 16 2019" -A x64 -B bin64 -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/msvc.cmake

## Quality Assurance

The development infrastructure for the library includes
these per-commit analyses:

* Coverage reports
* Benchmark performance comparisons
* Compilation and tests on Drone.io, Azure Pipelines, Appveyor
* Fuzzing using clang-llvm and machine learning

## License

Distributed under the Boost Software License, Version 1.0.
(See accompanying file [LICENSE_1_0.txt](LICENSE_1_0.txt) or copy at
https://www.boost.org/LICENSE_1_0.txt)

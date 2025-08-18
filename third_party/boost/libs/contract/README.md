Boost.Contract
==============

Contract programming for C++.
All contract programming features are supported: Subcontracting, class invariants (also static and volatile), postconditions (with old and return values), preconditions, customizable actions on assertion failure (e.g., terminate or throw), optional compilation and checking of assertions, disable assertions while already checking other assertions (to avoid infinite recursion), etc.

### License

Distributed under the [Boost Software License, Version 1.0](https://www.boost.org/LICENSE_1_0.txt).

### Properties

* C++11
* Shared Library / DLL with `BOOST_CONTRACT_DYN_LINK` (static library with `BOOST_CONTRACT_STATIC_LINK`, header-only also possible but not recommended, see `BOOST_CONTRACT_HEADER_ONLY` documentation for more information).

### Build Status

<!-- boost-ci/tools/makebadges.sh --project contract --appveyor bo914d458nsx83yw --codecov EL686wMU6K --coverity 15866 -->
| Branch          | GHA CI | Appveyor | Coverity Scan | codecov.io | Deps | Docs | Tests |
| :-------------: | ------ | -------- | ------------- | ---------- | ---- | ---- | ----- |
| [`master`](https://github.com/boostorg/contract/tree/master) | [![Build Status](https://github.com/boostorg/contract/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/boostorg/contract/actions?query=branch:master) | [![Build status](https://ci.appveyor.com/api/projects/status/bo914d458nsx83yw/branch/master?svg=true)](https://ci.appveyor.com/project/cppalliance/contract/branch/master) | [![Coverity Scan Build Status](https://scan.coverity.com/projects/15866/badge.svg)](https://scan.coverity.com/projects/boostorg-contract) | [![codecov](https://codecov.io/gh/boostorg/contract/branch/master/graph/badge.svg?token=EL686wMU6K)](https://codecov.io/gh/boostorg/contract/tree/master) | [![Deps](https://img.shields.io/badge/deps-master-brightgreen.svg)](https://pdimov.github.io/boostdep-report/master/contract.html) | [![Documentation](https://img.shields.io/badge/docs-master-brightgreen.svg)](https://www.boost.org/doc/libs/master/libs/contract) | [![Enter the Matrix](https://img.shields.io/badge/matrix-master-brightgreen.svg)](https://www.boost.org/development/tests/master/developer/contract.html)
| [`develop`](https://github.com/boostorg/contract/tree/develop) | [![Build Status](https://github.com/boostorg/contract/actions/workflows/ci.yml/badge.svg?branch=develop)](https://github.com/boostorg/contract/actions?query=branch:develop) | [![Build status](https://ci.appveyor.com/api/projects/status/bo914d458nsx83yw/branch/develop?svg=true)](https://ci.appveyor.com/project/cppalliance/contract/branch/develop) | [![Coverity Scan Build Status](https://scan.coverity.com/projects/15866/badge.svg)](https://scan.coverity.com/projects/boostorg-contract) | [![codecov](https://codecov.io/gh/boostorg/contract/branch/develop/graph/badge.svg?token=EL686wMU6K)](https://codecov.io/gh/boostorg/contract/tree/develop) | [![Deps](https://img.shields.io/badge/deps-develop-brightgreen.svg)](https://pdimov.github.io/boostdep-report/develop/contract.html) | [![Documentation](https://img.shields.io/badge/docs-develop-brightgreen.svg)](https://www.boost.org/doc/libs/develop/libs/contract) | [![Enter the Matrix](https://img.shields.io/badge/matrix-develop-brightgreen.svg)](https://www.boost.org/development/tests/develop/developer/contract.html)

### Directories

| Name        | Purpose                        |
| ----------- | ------------------------------ |
| `build`     | Build                          |
| `doc`       | Documentation                  |
| `example`   | Examples                       |
| `include`   | Header code                    |
| `meta`      | Integration with Boost         |
| `src`       | Source code                    |
| `test`      | Unit tests                     |

### More Information

* [Ask questions](https://stackoverflow.com/questions/ask?tags=c%2B%2B,boost,boost-contract).
* [Report bugs](https://github.com/boostorg/contract/issues): Be sure to mention Boost version, platform and compiler you are using. A small compilable code sample to reproduce the problem is always good as well.
* Submit your patches as pull requests against **develop** branch. Note that by submitting patches you agree to license your modifications under the [Boost Software License, Version 1.0](https://www.boost.org/LICENSE_1_0.txt).
* Discussions about the library are held on the [Boost developers mailing list](https://www.boost.org/community/groups.html#main). Be sure to read the [discussion policy](https://www.boost.org/community/policy.html) before posting and add the `[contract]` text at the beginning of the subject line.


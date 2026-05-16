Boost.Interprocess
==========

Boost.Interprocess simplifies the use of common interprocess communication and synchronization mechanisms and offers a wide range of them:

* Shared memory.
* Memory-mapped files.
* Semaphores, mutexes, condition variables and upgradable mutex types to place
  them in shared memory and memory mapped files.
* Named versions of those synchronization objects, similar to UNIX/Windows
  sem_open/CreateSemaphore API.
* File locking.
* Relative pointers.
* Message queues.

Also offers higher-level interprocess mechanisms to dynamically allocate portions of a shared memory or a memory mapped file.
Using these mechanisms, it offers useful tools to construct C++ objects, including STL-like containers, in shared memory and memory mapped files.

### License

Distributed under the [Boost Software License, Version 1.0](http://www.boost.org/LICENSE_1_0.txt).

### Properties

* C++03
* Header-only.

### Build Status

Branch          | Deps | Docs | Tests |
:-------------: | ---- | ---- | ----- |
[`master`](https://github.com/boostorg/interprocess/tree/master) | [![Deps](https://img.shields.io/badge/deps-master-brightgreen.svg)](https://pdimov.github.io/boostdep-report/master/interprocess.html) | [![Documentation](https://img.shields.io/badge/docs-master-brightgreen.svg)](http://www.boost.org/doc/libs/master/doc/html/interprocess.html) | [![Enter the Matrix](https://img.shields.io/badge/matrix-master-brightgreen.svg)](http://www.boost.org/development/tests/master/developer/interprocess.html)
[`develop`](https://github.com/boostorg/interprocess/tree/develop) | [![Deps](https://img.shields.io/badge/deps-develop-brightgreen.svg)](https://pdimov.github.io/boostdep-report/develop/interprocess.html) | [![Documentation](https://img.shields.io/badge/docs-develop-brightgreen.svg)](http://www.boost.org/doc/libs/develop/doc/html/interprocess.html) | [![Enter the Matrix](https://img.shields.io/badge/matrix-develop-brightgreen.svg)](http://www.boost.org/development/tests/develop/developer/interprocess.html)

### Directories

| Name        | Purpose                        |
| ----------- | ------------------------------ |
| `doc`       | documentation                  |
| `example`   | examples                       |
| `extra`     | extra utilities				   |
| `include`   | headers                        |
| `test`      | unit tests                     |

### More information

* [Ask questions](http://stackoverflow.com/questions/ask?tags=c%2B%2B,boost,boost-interprocess)
* [Report bugs](https://github.com/boostorg/interprocess/issues): Be sure to mention Boost version, platform and compiler you're using. A small compilable code sample to reproduce the problem is always good as well.
* Submit your patches as pull requests against **develop** branch. Note that by submitting patches you agree to license your modifications under the [Boost Software License, Version 1.0](http://www.boost.org/LICENSE_1_0.txt).
* Discussions about the library are held on the [Boost developers mailing list](http://www.boost.org/community/groups.html#main). Be sure to read the [discussion policy](http://www.boost.org/community/policy.html) before posting and add the `[interprocess]` tag at the beginning of the subject line.


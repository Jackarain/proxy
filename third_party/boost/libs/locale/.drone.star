# Copyright 2020 Rene Rivera
# Copyright 2022-2023 Alexander Grund
#
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

# For Drone CI we use the Starlark scripting language to reduce duplication.
# As the yaml syntax for Drone CI is rather limited.

# Base environment for all jobs
globalenv={'B2_CI_VERSION': '1', 'B2_VARIANT': 'debug,release'}

# Wrapper function to apply the globalenv to all jobs
def job(
        # job specific environment options
        env={},
        **kwargs):
  real_env = dict(globalenv)
  real_env.update(env)
  return job_impl(env=real_env, **kwargs)

def main(ctx):
  return [
    job(compiler='clang-13',  cxxstd='11,14,17,20,2b', os='ubuntu-22.04', install='libicu-dev'),
    job(compiler='clang-14',  cxxstd='11,14,17,20,2b', os='ubuntu-22.04', install='libicu-dev'),
    job(compiler='clang-15',  cxxstd='11,14,17,20,2b', os='ubuntu-22.04', install='libicu-dev', add_llvm=True),

    job(compiler='gcc-11',    cxxstd='11,14,17,20,2b', os='ubuntu-22.04', install='libicu-dev'),
    job(compiler='gcc-12',    cxxstd='11,14,17,20,2b', os='ubuntu-22.04', install='libicu-dev'),

    # Sanitizers
    job(name='ASAN',  asan=True,
        compiler='gcc-12',    cxxstd='11,14,17,20', os='ubuntu-22.04', install='libicu-dev'),
    job(name='UBSAN', ubsan=True,
        compiler='gcc-12',    cxxstd='11,14,17,20', os='ubuntu-22.04', install='libicu-dev'),
    job(name='Clang 14 w/ sanitizers', asan=True, ubsan=True,
        compiler='clang-14',  cxxstd='11,14,17,20', os='ubuntu-22.04', install='libicu-dev'),

    # libc++
    job(compiler='clang-15',  cxxstd='11,14,17,20', os='ubuntu-22.04', stdlib='libc++', install='libicu-dev libc++-15-dev libc++abi-15-dev', add_llvm=True),

    # FreeBSD
    job(compiler='clang-10',  cxxstd='11,14,17,20', os='freebsd-13.1'),
    job(compiler='clang-15',  cxxstd='11,14,17,20', os='freebsd-13.1'),
    # ICU is linked against libc++, so either don't use ICU or use libc++. The latter doesn't seem to work well (segfaults)
    job(compiler='gcc-11',    cxxstd='11,14,17,20', os='freebsd-13.1', testflags='boost.locale.icu=off', linkflags='-Wl,-rpath=/usr/local/lib/gcc11'),
    # OSX
    job(compiler='clang',     cxxstd='11,14,17,2a',    os='osx-xcode-10.1'),
    job(compiler='clang',     cxxstd='11,14,17,2a',    os='osx-xcode-10.3'),
    job(compiler='clang',     cxxstd='11,14,17,2a',    os='osx-xcode-12'),
    job(compiler='clang',     cxxstd='11,14,17,20',    os='osx-xcode-12.5.1'),
    job(compiler='clang',     cxxstd='11,14,17,20',    os='osx-xcode-13.4.1'),
    # Don't work yet -> #206 ?
    # job(compiler='clang',     cxxstd='11,14,17,20,2b', os='osx-xcode-14.3.1'),
    # job(compiler='clang',     cxxstd='11,14,17,20,2b', os='osx-xcode-15.0.1'),
    # ARM64
    job(compiler='clang-12',  cxxstd='11,14,17,20', os='ubuntu-20.04', arch='arm64', add_llvm=True),
    job(compiler='gcc-11',    cxxstd='11,14,17,20', os='ubuntu-20.04', arch='arm64'),
    # S390x
    job(compiler='clang-12',  cxxstd='11,14,17,20', os='ubuntu-20.04', arch='s390x', add_llvm=True),
    job(compiler='gcc-11',    cxxstd='11,14,17,20', os='ubuntu-20.04', arch='s390x'),
    # Windows
    job(compiler='msvc-14.3', cxxstd='14,17,20,latest', os='windows'),
  ]

# from https://github.com/boostorg/boost-ci
load("@boost_ci//ci/drone/:functions.star", "linux_cxx", "windows_cxx", "osx_cxx", "freebsd_cxx", "job_impl")

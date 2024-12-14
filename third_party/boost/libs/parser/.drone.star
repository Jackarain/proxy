# Use, modification, and distribution are
# subject to the Boost Software License, Version 1.0. (See accompanying
# file LICENSE.txt)
#
# Copyright Rene Rivera 2020.
# Copyright Alexander Grund 2022.
# Copyright T. Zachary Laine 2024.

# For Drone CI we use the Starlark scripting language to reduce duplication.
# As the yaml syntax for Drone CI is rather limited.

# Base environment for all jobs
globalenv={'B2_CI_VERSION': '1', 'B2_VARIANT': 'release'}

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
    job(compiler='clang-11',  cxxstd='17',       os='ubuntu-22.04'),
    job(compiler='clang-12',  cxxstd='17',       os='ubuntu-22.04'),
    job(compiler='clang-13',  cxxstd='17,20',    os='ubuntu-22.04'),
    job(compiler='clang-14',  cxxstd='17,20',    os='ubuntu-22.04'),
    job(compiler='clang-15',  cxxstd='17,20',    os='ubuntu-22.04', add_llvm=True),

    job(compiler='gcc-9',     cxxstd='17',       os='ubuntu-18.04'),
    job(compiler='gcc-10',    cxxstd='17,20',    os='ubuntu-22.04'),
    job(compiler='gcc-11',    cxxstd='17,20,2b', os='ubuntu-22.04'),
    job(compiler='gcc-12',    cxxstd='17,20,2b', os='ubuntu-22.04'),

    # Sanitizers
    job(name='ASAN',  asan=True,
        compiler='gcc-12',    cxxstd='17,20', os='ubuntu-22.04'),
    job(name='UBSAN', ubsan=True,
        compiler='gcc-12',    cxxstd='17,20', os='ubuntu-22.04'),
    job(name='TSAN',  tsan=True,
        compiler='gcc-12',    cxxstd='17,20', os='ubuntu-22.04'),
    job(name='Clang 14 w/ sanitizers', asan=True, ubsan=True,
        compiler='clang-14',  cxxstd='17', os='ubuntu-22.04'),
    job(name='Clang 11 libc++ w/ sanitizers', asan=True, ubsan=True, # libc++-11 is the latest working with ASAN: https://github.com/llvm/llvm-project/issues/59432
        compiler='clang-11',  cxxstd='17', os='ubuntu-20.04', stdlib='libc++', install='libc++-11-dev libc++abi-11-dev'),

    # libc++
    job(compiler='clang-10',  cxxstd='17',    os='ubuntu-20.04', stdlib='libc++', install='libc++-10-dev libc++abi-10-dev'),
    job(compiler='clang-11',  cxxstd='17',    os='ubuntu-20.04', stdlib='libc++', install='libc++-11-dev libc++abi-11-dev'),
    job(compiler='clang-12',  cxxstd='17',    os='ubuntu-22.04', stdlib='libc++', install='libc++-12-dev libc++abi-12-dev libunwind-12-dev'),
    job(compiler='clang-13',  cxxstd='17,20', os='ubuntu-22.04', stdlib='libc++', install='libc++-13-dev libc++abi-13-dev'),
    job(compiler='clang-14',  cxxstd='17,20', os='ubuntu-22.04', stdlib='libc++', install='libc++-14-dev libc++abi-14-dev'),
    job(compiler='clang-15',  cxxstd='17,20', os='ubuntu-22.04', stdlib='libc++', install='libc++-15-dev libc++abi-15-dev', add_llvm=True),

    # FreeBSD
    job(compiler='clang-10',  cxxstd='17', os='freebsd-13.1'),
    job(compiler='clang-15',  cxxstd='17', os='freebsd-13.1'),
    job(compiler='gcc-11',    cxxstd='17,20', os='freebsd-13.1', linkflags='-Wl,-rpath=/usr/local/lib/gcc11'),
    # OSX
    job(compiler='clang',     cxxstd='17',       os='osx-xcode-10.3'),
    job(compiler='clang',     cxxstd='17',       os='osx-xcode-11.1'),
    job(compiler='clang',     cxxstd='17',       os='osx-xcode-11.7'),
    job(compiler='clang',     cxxstd='17',       os='osx-xcode-12'),
    job(compiler='clang',     cxxstd='17',       os='osx-xcode-12.5.1'),
    job(compiler='clang',     cxxstd='17,20',    os='osx-xcode-13.0'),
    job(compiler='clang',     cxxstd='17,20',    os='osx-xcode-13.4.1'),
    job(compiler='clang',     cxxstd='17,20',    os='osx-xcode-14.0'),
    job(compiler='clang',     cxxstd='17,20,2b', os='osx-xcode-14.3.1'),
    job(compiler='clang',     cxxstd='17,20,2b', os='osx-xcode-15.0.1'),
    # ARM64
    job(compiler='gcc-11',    cxxstd='17,20', os='ubuntu-20.04', arch='arm64'),
    # S390x
    job(compiler='gcc-11',    cxxstd='17,20', os='ubuntu-20.04', arch='s390x'),
  ]

# from https://github.com/boostorg/boost-ci
load("@boost_ci//ci/drone/:functions.star", "linux_cxx", "windows_cxx", "osx_cxx", "freebsd_cxx", "job_impl")

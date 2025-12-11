# Use, modification, and distribution are
# subject to the Boost Software License, Version 1.0. (See accompanying
# file LICENSE.txt)
#
# Copyright (c) 2020 Rene Rivera
# Copyright (c) 2022 Alan de Freitas
# Copyright (c) 2022-2025 Alexander Grund

# For Drone CI we use the Starlark scripting language to reduce duplication.
# As the yaml syntax for Drone CI is rather limited.

# Base environment for all jobs
globalenv={'B2_VARIANT': 'release'}

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
    job(compiler='clang-3.8', cxxstd='11,14',          os='ubuntu-16.04'),
    job(compiler='clang-3.9', cxxstd='11,14',          os='ubuntu-18.04'),
    job(compiler='clang-4.0', cxxstd='11,14',          os='ubuntu-18.04'),
    job(compiler='clang-5.0', cxxstd='11,14,1z',       os='ubuntu-18.04'),
    job(compiler='clang-6.0', cxxstd='11,14,17',       os='ubuntu-18.04'),
    job(compiler='clang-7',   cxxstd='11,14,17',       os='ubuntu-18.04'),
    job(compiler='clang-8',   cxxstd='11,14,17,2a',    os='ubuntu-18.04'),
    job(compiler='clang-9',   cxxstd='11,14,17,2a',    os='ubuntu-18.04'),
    job(compiler='clang-10',  cxxstd='11,14,17,2a',    os='ubuntu-18.04'),
    job(compiler='clang-11',  cxxstd='11,14,17,2a',    os='ubuntu-22.04'),
    job(compiler='clang-12',  cxxstd='11,14,17,20',    os='ubuntu-22.04'),
    job(compiler='clang-13',  cxxstd='11,14,17,20,2b', os='ubuntu-22.04'),
    job(compiler='clang-14',  cxxstd='11,14,17,20,2b', os='ubuntu-22.04'),
    job(compiler='clang-15',  cxxstd='11,14,17,20,2b', os='ubuntu-22.04', add_llvm=True,
        env={'B2_CXXFLAGS': '-Werror'}),
    job(name='Clang 15 standalone', compiler='clang-15',  cxxstd='17,20,2b', os='ubuntu-22.04', add_llvm=True,
        env={'B2_CXXFLAGS': '-Werror', 'B2_DEFINES': 'BOOST_STATIC_STRING_STANDALONE'}),

    job(compiler='gcc-4.8',   cxxstd='11',             os='ubuntu-16.04'),
    job(compiler='gcc-4.9',   cxxstd='11',             os='ubuntu-16.04'),
    job(compiler='gcc-5',     cxxstd='11,14,1z',       os='ubuntu-18.04'),
    job(compiler='gcc-6',     cxxstd='11,14,1z',       os='ubuntu-18.04'),
    job(compiler='gcc-7',     cxxstd='11,14,1z',       os='ubuntu-18.04'),
    job(compiler='gcc-8',     cxxstd='11,14,17,2a',    os='ubuntu-18.04'),
    job(compiler='gcc-9',     cxxstd='11,14,17,2a',    os='ubuntu-18.04'),
    job(compiler='gcc-10',    cxxstd='11,14,17,20',    os='ubuntu-22.04'),
    job(compiler='gcc-11',    cxxstd='11,14,17,20,2b', os='ubuntu-22.04'),
    job(compiler='gcc-12',    cxxstd='11,14,17,20,2b', os='ubuntu-22.04'),

    job(name='Coverage', buildtype='codecov', buildscript='codecov_coveralls', env={'LCOV_BRANCH_COVERAGE': 1, "COVERALLS_REPO_TOKEN": {"from_secret": "coveralls_repo_token"}},
        compiler='gcc-8',     cxxstd='11,14,17,2a',    os='ubuntu-18.04'),
    # Sanitizers
    job(name='ASAN',  asan=True,
        compiler='gcc-12',    cxxstd='11,14,17,20',    os='ubuntu-22.04'),
    job(name='UBSAN', ubsan=True,
        compiler='gcc-12',    cxxstd='11,14,17,20',    os='ubuntu-22.04'),
    job(name='TSAN',  tsan=True,
        compiler='gcc-12',    cxxstd='11,14,17,20',    os='ubuntu-22.04'),
    job(name='Clang 14 w/ sanitizers', asan=True, ubsan=True,
        compiler='clang-14',  cxxstd='11,14,17,20',    os='ubuntu-22.04'),
    job(name='Clang 11 libc++ w/ sanitizers', asan=True, ubsan=True, # libc++-11 is the latest working with ASAN: https://github.com/llvm/llvm-project/issues/59432
        compiler='clang-11',  cxxstd='11,14,17,20',    os='ubuntu-20.04', stdlib='libc++', install='libc++-11-dev libc++abi-11-dev'),
    job(name='Valgrind', valgrind=True,
        compiler='clang-6.0', cxxstd='11,14,1z',       os='ubuntu-18.04', install='libc6-dbg libc++-dev libstdc++-8-dev'),

    # libc++
    job(compiler='clang-6.0', cxxstd='11,14,17,2a',    os='ubuntu-18.04', stdlib='libc++', install='libc++-dev libc++abi-dev'),
    job(compiler='clang-7',   cxxstd='11,14,17,2a',    os='ubuntu-20.04', stdlib='libc++', install='libc++-7-dev libc++abi-7-dev'),
    job(compiler='clang-8',   cxxstd='11,14,17,2a',    os='ubuntu-20.04', stdlib='libc++', install='libc++-8-dev libc++abi-8-dev'),
    job(compiler='clang-9',   cxxstd='11,14,17,2a',    os='ubuntu-20.04', stdlib='libc++', install='libc++-9-dev libc++abi-9-dev'),
    job(compiler='clang-10',  cxxstd='11,14,17,20',    os='ubuntu-20.04', stdlib='libc++', install='libc++-10-dev libc++abi-10-dev'),
    job(compiler='clang-11',  cxxstd='11,14,17,20',    os='ubuntu-20.04', stdlib='libc++', install='libc++-11-dev libc++abi-11-dev'),
    job(compiler='clang-12',  cxxstd='11,14,17,20',    os='ubuntu-22.04', stdlib='libc++', install='libc++-12-dev libc++abi-12-dev libunwind-12-dev'),
    job(compiler='clang-13',  cxxstd='11,14,17,20',    os='ubuntu-22.04', stdlib='libc++', install='libc++-13-dev libc++abi-13-dev'),
    job(compiler='clang-14',  cxxstd='11,14,17,20',    os='ubuntu-22.04', stdlib='libc++', install='libc++-14-dev libc++abi-14-dev'),
    job(compiler='clang-15',  cxxstd='11,14,17,20',    os='ubuntu-22.04', stdlib='libc++', install='libc++-15-dev libc++abi-15-dev', add_llvm=True),

    # FreeBSD
    job(compiler='clang-10',  cxxstd='11,14,17,20',    os='freebsd-13.1'),
    job(compiler='clang-15',  cxxstd='11,14,17,20',    os='freebsd-13.1'),
    job(compiler='gcc-11',    cxxstd='11,14,17,20',    os='freebsd-13.1', linkflags='-Wl,-rpath=/usr/local/lib/gcc11'),
    # OSX
    job(compiler='clang',     cxxstd='11,14,17,2a',    os='osx-xcode-10.1'),
    job(compiler='clang',     cxxstd='11,14,17,2a',    os='osx-xcode-10.3'),
    job(compiler='clang',     cxxstd='11,14,17,2a',    os='osx-xcode-11.1'),
    job(compiler='clang',     cxxstd='11,14,17,2a',    os='osx-xcode-11.7'),
    job(compiler='clang',     cxxstd='11,14,17,2a',    os='osx-xcode-12'),
    job(compiler='clang',     cxxstd='11,14,17,20',    os='osx-xcode-12.5.1'),
    job(compiler='clang',     cxxstd='11,14,17,20',    os='osx-xcode-13.0'),
    job(compiler='clang',     cxxstd='11,14,17,20',    os='osx-xcode-13.4.1'),
    job(compiler='clang',     cxxstd='11,14,17,20,2b', os='osx-xcode-14.0'),
    job(compiler='clang',     cxxstd='11,14,17,20,2b', os='osx-xcode-14.3.1'),
    job(compiler='clang',     cxxstd='11,14,17,20,2b', os='osx-xcode-15.0.1'),
    # ARM64
    job(compiler='clang-12',  cxxstd='11,14,17,20',    os='ubuntu-20.04', arch='arm64', add_llvm=True),
    job(compiler='gcc-11',    cxxstd='11,14,17,20',    os='ubuntu-20.04', arch='arm64'),
    # S390x
    job(compiler='clang-12',  cxxstd='11,14,17,20',    os='ubuntu-20.04', arch='s390x', add_llvm=True),
    job(compiler='gcc-11',    cxxstd='11,14,17,20',    os='ubuntu-20.04', arch='s390x'),
    # Windows
    job(compiler='msvc-14.0', cxxstd=None,              os='windows', env={'B2_DONT_EMBED_MANIFEST': 1}),
    job(compiler='msvc-14.1', cxxstd=None,              os='windows'),
    job(compiler='msvc-14.2', cxxstd=None,              os='windows'),
    job(compiler='msvc-14.3', cxxstd=None,              os='windows'),
    job(compiler='msvc-14.0', cxxstd='14,17,20',        os='windows', env={'B2_DONT_EMBED_MANIFEST': 1}),
    job(compiler='msvc-14.1', cxxstd='14,17,20',        os='windows'),
    job(compiler='msvc-14.2', cxxstd='14,17,20',        os='windows'),
    job(compiler='msvc-14.3', cxxstd='14,17,20,latest', os='windows'),
  ]

# from https://github.com/boostorg/boost-ci
load("@boost_ci//ci/drone/:functions.star", "linux_cxx", "windows_cxx", "osx_cxx", "freebsd_cxx", "job_impl")

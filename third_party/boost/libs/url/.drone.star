# Use, modification, and distribution are
# subject to the Boost Software License, Version 1.0. (See accompanying
# file LICENSE.txt)
#
# Copyright Rene Rivera 2020.
# Copyright Alan de Freitas 2022.

# For Drone CI we use the Starlark scripting language to reduce duplication.
# As the yaml syntax for Drone CI is rather limited.
#
#

def main(ctx):
    return generate(
        # Compilers
        [
            'gcc >=4.8',
            'clang >=3.8',
            'msvc >=14.1',
            'arm64-gcc latest',
            's390x-gcc latest',
            # 'freebsd-gcc latest',
            'apple-clang *',
            'arm64-clang latest',
            's390x-clang latest',
            'freebsd-clang latest',
            'x86-msvc latest'
        ],
        # Standards
        '>=11',
        # Asan is delegated to GHA
        asan=False,
        docs=False,
        cache_dir='cache') + [
               linux_cxx("GCC 12 (no-mutex)", "g++-12", packages="g++-12", buildscript="drone", buildtype="boost",
                         image="cppalliance/droneubuntu2204:1",
                         environment={'B2_TOOLSET': 'gcc-12', 'B2_DEFINES': 'BOOST_URL_DISABLE_THREADS=1',
                                      'B2_CXXSTD': '17'}, globalenv={'B2_CI_VERSION': '1', 'B2_VARIANT': 'release'}),
           ]


# from https://github.com/cppalliance/ci-automation
load("@ci_automation//ci/drone/:functions.star", "linux_cxx", "windows_cxx", "osx_cxx", "freebsd_cxx", "generate")


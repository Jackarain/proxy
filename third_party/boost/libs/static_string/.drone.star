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
globalenv={'B2_CI_VERSION': '1', 'B2_VARIANT': 'release'}
linuxglobalimage="cppalliance/droneubuntu1804:1"
windowsglobalimage="cppalliance/dronevs2019"

def main(ctx):
  return [
  # Priorities: (no 2a, no betas)
  #
  # coverage
  # latest gcc: 17,20
  # latest clang: 17,20
  # oldest gcc: 11
  # oldest clang: 11
  # asan
  # tsan
  # ubsan
  # valgrind
  # arm64
  # s390x
  # docs
  # cmake superproject
  # cmake install
  # (...the rest)

  # Coverage
  linux_cxx("Coverage", "g++-8", packages="g++-8", buildscript="drone", buildtype="codecov", image=linuxglobalimage, environment={'COMMENT': 'codecov.io', 'LCOV_BRANCH_COVERAGE': '0', 'B2_CXXSTD': '11', 'B2_TOOLSET': 'gcc-8', 'B2_DEFINES': 'BOOST_NO_STRESS_TEST=1', "CODECOV_TOKEN": {"from_secret": "codecov_token"}, "COVERALLS_REPO_TOKEN": {"from_secret": "coveralls_repo_token"}}, globalenv=globalenv),

  # Latest gcc
  linux_cxx("GCC 12: C++17,20", "g++-12", packages="g++-12", buildscript="drone", buildtype="boost", image="cppalliance/droneubuntu2204:1", environment={'B2_TOOLSET': 'gcc-12', 'B2_CXXFLAGS': '-Werror', 'B2_CXXSTD': '17,20'}, globalenv=globalenv),
  linux_cxx("GCC 12: C++17,20 Standalone", "g++-12", packages="g++-12", buildscript="drone", buildtype="boost", image="cppalliance/droneubuntu2204:1", environment={'B2_TOOLSET': 'gcc-12', 'B2_CXXFLAGS': '-Werror', 'B2_DEFINES': 'define=BOOST_STATIC_STRING_STANDALONE', 'B2_CXXSTD': '17,20'}, globalenv=globalenv),

  # Latest clang
  linux_cxx("Clang 15: C++17,20", "clang++-15", packages="clang-15 libstdc++-10-dev", llvm_os="jammy", llvm_ver="15", buildscript="drone", buildtype="boost", image="cppalliance/droneubuntu2204:1", environment={'B2_TOOLSET': 'clang-15', 'B2_CXXFLAGS': '-Werror', 'B2_CXXSTD': '17,20'}, globalenv=globalenv),
  linux_cxx("Clang 15: C++17,20 Standalone", "clang++-15", packages="clang-15 libstdc++-10-dev", llvm_os="jammy", llvm_ver="15", buildscript="drone", buildtype="boost", image="cppalliance/droneubuntu2204:1", environment={'B2_TOOLSET': 'clang-15', 'B2_CXXFLAGS': '-Werror', 'B2_DEFINES': 'define=BOOST_STATIC_STRING_STANDALONE', 'B2_CXXSTD': '17,20'}, globalenv=globalenv),

  # Oldest compilers
  linux_cxx("GCC 4.8: C++11", "g++-4.8", packages="g++-4.8", buildscript="drone", buildtype="boost", image="cppalliance/droneubuntu1604:1", environment={'B2_TOOLSET': 'gcc-4.8', 'B2_CXXFLAGS': '-Werror', 'B2_CXXSTD': '11'}, globalenv=globalenv),
  linux_cxx("GCC 4.8: C++11 Standalone", "g++-4.8", packages="g++-4.8", buildscript="drone", buildtype="boost", image="cppalliance/droneubuntu1604:1", environment={'B2_TOOLSET': 'gcc-4.8', 'B2_CXXFLAGS': '-Werror', 'B2_DEFINES': 'define=BOOST_STATIC_STRING_STANDALONE', 'B2_CXXSTD': '11'}, globalenv=globalenv),
  linux_cxx("Clang 3.8: C++11", "clang++-3.8", packages="clang-3.8", buildscript="drone", buildtype="boost", image="cppalliance/droneubuntu1604:1", environment={'B2_VARIANT': 'debug', 'B2_CXXFLAGS': '-Werror', 'B2_TOOLSET': 'clang-3.8', 'B2_CXXSTD': '11'}, globalenv=globalenv),
  linux_cxx("Clang 3.8: C++11 Standalone", "clang++-3.8", packages="clang-3.8", buildscript="drone", buildtype="boost", image="cppalliance/droneubuntu1604:1", environment={'B2_VARIANT': 'debug', 'B2_CXXFLAGS': '-Werror', 'B2_TOOLSET': 'clang-3.8', 'B2_DEFINES': 'define=BOOST_STATIC_STRING_STANDALONE', 'B2_CXXSTD': '11'}, globalenv=globalenv),

  # Sanitizers + Valgrind
  linux_cxx("ASan", "g++-12", packages="g++-12", buildscript="drone", buildtype="boost", image="cppalliance/droneubuntu2204:1", environment={'COMMENT': 'asan', 'B2_VARIANT': 'debug', 'B2_TOOLSET': 'gcc-12', 'B2_CXXSTD': '11,14,17', 'B2_ASAN': '1', 'B2_DEFINES': 'BOOST_NO_STRESS_TEST=1', 'DRONE_EXTRA_PRIVILEGED': 'True'}, globalenv=globalenv, privileged=True),
  linux_cxx("TSan", "g++-12", packages="g++-12", buildscript="drone", buildtype="boost", image="cppalliance/droneubuntu2204:1", environment={'COMMENT': 'tsan', 'B2_VARIANT': 'debug', 'B2_TOOLSET': 'gcc-12', 'B2_CXXSTD': '11,14,17', 'B2_TSAN': '1', 'B2_DEFINES': 'BOOST_NO_STRESS_TEST=1'}, globalenv=globalenv),
  linux_cxx("UBSan", "g++-12", packages="g++-12", buildscript="drone", buildtype="boost", image="cppalliance/droneubuntu2204:1", environment={'COMMENT': 'ubsan', 'B2_VARIANT': 'debug', 'B2_TOOLSET': 'gcc-12', 'B2_CXXSTD': '11,14,17', 'B2_UBSAN': '1', 'B2_DEFINES': 'define=BOOST_NO_STRESS_TEST=1', 'B2_LINKFLAGS': '-fuse-ld=gold'}, globalenv=globalenv),
  linux_cxx("Valgrind", "clang++-14", packages="clang-14 libc6-dbg libc++-dev libstdc++-9-dev", llvm_os="jammy", llvm_ver="14", buildscript="drone", buildtype="valgrind", image="cppalliance/droneubuntu2204:1", environment={'COMMENT': 'valgrind', 'B2_TOOLSET': 'clang-14', 'B2_CXXSTD': '11,14,17', 'B2_DEFINES': 'BOOST_NO_STRESS_TEST=1', 'B2_VARIANT': 'debug', 'B2_TESTFLAGS': 'testing.launcher=valgrind', 'VALGRIND_OPTS': '--error-exitcode=1'}, globalenv=globalenv),

  # arm64 (unsigned char)
  linux_cxx("ARM64: GCC 11", "g++-11", packages="g++-11", buildscript="drone", buildtype="boost", image="cppalliance/droneubuntu2004:multiarch", environment={'B2_TOOLSET': 'gcc-11', 'B2_CXXFLAGS': '-Werror', 'B2_CXXSTD': '17,20'}, arch="arm64", globalenv=globalenv),

  # s390x
  linux_cxx("S390x: Clang 12", "clang++-12", packages="clang-12 libstdc++-9-dev", llvm_os="focal", llvm_ver="12", buildtype="boost", buildscript="drone", image="cppalliance/droneubuntu2004:multiarch", environment={'B2_TOOLSET': 'clang-12', 'B2_CXXFLAGS': '-Werror', 'B2_CXXSTD': '17,20'}, arch="s390x", globalenv=globalenv),

  # Documentation
  # linux_cxx("Docs", "g++", packages="docbook docbook-xml docbook-xsl xsltproc libsaxonhe-java default-jre-headless flex libfl-dev bison unzip rsync", buildtype="docs", buildscript="drone", image="cppalliance/droneubuntu1804:1", environment={'COMMENT': 'docs'}, globalenv=globalenv),

  # CMake tests (https://github.com/boostorg/boost-ci)
  ## Compiling as part of the boost superproject
  linux_cxx("CMake Superproject", "g++", packages="", buildscript="drone", buildtype="cmake-superproject", image="cppalliance/droneubuntu1804:1", globalenv=globalenv),
  ## Installing
  # linux_cxx("CMake Install", "g++", packages="", buildscript="drone", buildtype="cmake1", image="cppalliance/droneubuntu1804:1", environment={'CMAKE_INSTALL_TEST': '1'}, globalenv=globalenv),

  # ------------------------------------------------------------------

  # OSX
  osx_cxx("OSX: Clang", "g++", packages="", buildscript="drone", buildtype="boost", xcode_version="13.4.1", environment={'B2_TOOLSET': 'clang', 'B2_CXXFLAGS': '-Werror', 'B2_CXXSTD': '11,17'}, globalenv=globalenv),

  # GCC (All other versions)
  linux_cxx("GCC 4.9: C++11", "g++-4.9", packages="g++-4.9", buildscript="drone", buildtype="boost", image="cppalliance/droneubuntu1604:1", environment={'B2_TOOLSET': 'gcc-4.9', 'B2_CXXFLAGS': '-Werror', 'B2_CXXSTD': '11'}, globalenv=globalenv),
  linux_cxx("GCC 5: C++11", "g++-5", packages="g++-5", buildscript="drone", buildtype="boost", image=linuxglobalimage, environment={'B2_TOOLSET': 'gcc-5', 'B2_CXXFLAGS': '-Werror', 'B2_CXXSTD': '11'}, globalenv=globalenv),
  linux_cxx("GCC 6: C++11,14", "g++-6", packages="g++-6", buildscript="drone", buildtype="boost", image=linuxglobalimage, environment={'B2_TOOLSET': 'gcc-6', 'B2_CXXFLAGS': '-Werror', 'B2_CXXSTD': '11,14'}, globalenv=globalenv),
  linux_cxx("GCC 7: C++14,17", "g++-7", packages="g++-7", buildscript="drone", buildtype="boost", image=linuxglobalimage, environment={'B2_TOOLSET': 'gcc-7', 'B2_CXXFLAGS': '-Werror', 'B2_CXXSTD': '14,17'}, globalenv=globalenv),
  linux_cxx("GCC 8: C++17", "g++-8", packages="g++-8", buildscript="drone", buildtype="boost", image=linuxglobalimage, environment={'B2_TOOLSET': 'gcc-8', 'B2_CXXFLAGS': '-Werror', 'B2_CXXSTD': '17'}, globalenv=globalenv),
  linux_cxx("GCC 9: C++17", "g++-9", packages="g++-9", buildscript="drone", buildtype="boost", image=linuxglobalimage, environment={'B2_TOOLSET': 'gcc-9', 'B2_CXXFLAGS': '-Werror', 'B2_CXXSTD': '17'}, globalenv=globalenv),
  linux_cxx("GCC 10: C++17", "g++-10", packages="g++-10", buildscript="drone", buildtype="boost", image=linuxglobalimage, environment={'B2_TOOLSET': 'gcc-10', 'B2_CXXFLAGS': '-Werror', 'B2_CXXSTD': '17'}, globalenv=globalenv),
  linux_cxx("GCC 11: C++17,20", "g++-11", packages="g++-11", buildscript="drone", buildtype="boost", image=linuxglobalimage, environment={'B2_TOOLSET': 'gcc-11', 'B2_CXXFLAGS': '-Werror', 'B2_CXXSTD': '17,20'}, globalenv=globalenv),

  # Clang (All other versions)
  linux_cxx("Clang 4.0: C++11,14", "clang++-4.0", packages="clang-4.0 libstdc++-6-dev", llvm_os="xenial", llvm_ver="4.0", buildscript="drone", buildtype="boost", image="cppalliance/droneubuntu1804:1", environment={'B2_TOOLSET': 'clang-4.0', 'B2_CXXFLAGS': '', 'B2_CXXSTD': '11,14'}, globalenv=globalenv),
  linux_cxx("Clang 5.0: C++11,14", "clang++-5.0", packages="clang-5.0 libstdc++-7-dev", llvm_os="bionic", llvm_ver="5.0", buildscript="drone", buildtype="boost", image=linuxglobalimage, environment={'B2_TOOLSET': 'clang-5.0', 'B2_CXXFLAGS': '', 'B2_CXXSTD': '11,14'}, globalenv=globalenv),
  linux_cxx("Clang 6.0: C++11,14", "clang++-6.0", packages="clang-6.0 libc6-dbg libc++-dev libc++abi-dev libstdc++-8-dev", llvm_os="bionic", llvm_ver="6.0", buildscript="drone", buildtype="boost", image=linuxglobalimage, environment={'B2_TOOLSET': 'clang-6.0', 'B2_CXXFLAGS': '-Werror', 'B2_CXXSTD': '11,14', 'B2_STDLIB': 'libc++'}, globalenv=globalenv),
  linux_cxx("Clang 6.0: C++14,17", "clang++-6.0", packages="clang-6.0 libc6-dbg libc++-dev libc++abi-dev libstdc++-8-dev", llvm_os="bionic", llvm_ver="6.0", buildscript="drone", buildtype="boost", image=linuxglobalimage, environment={'B2_TOOLSET': 'clang-6.0', 'B2_CXXFLAGS': '-Werror', 'B2_CXXSTD': '14,17'}, globalenv=globalenv),
  linux_cxx("Clang 7: C++17", "clang++-7", packages="clang-7 libc6-dbg libc++-dev libstdc++-8-dev", llvm_os="bionic", llvm_ver="7", buildscript="drone", buildtype="boost", image=linuxglobalimage, environment={'B2_TOOLSET': 'clang-7', 'B2_CXXFLAGS': '-Werror', 'B2_CXXSTD': '17'}, globalenv=globalenv),
  linux_cxx("Clang 8: C++17", "clang++-8", packages="clang-8 libc6-dbg libc++-dev libstdc++-8-dev", llvm_os="bionic", llvm_ver="8", buildscript="drone", buildtype="boost", image=linuxglobalimage, environment={'B2_TOOLSET': 'clang-8', 'B2_CXXFLAGS': '-Werror', 'B2_CXXSTD': '17'}, globalenv=globalenv),
  linux_cxx("Clang 9: C++14,17", "clang++-9", packages="clang-9 libstdc++-9-dev", llvm_os="bionic", llvm_ver="9", buildscript="drone", buildtype="boost", image=linuxglobalimage, environment={'B2_TOOLSET': 'clang-9', 'B2_CXXFLAGS': '-Werror', 'B2_CXXSTD': '14,17'}, globalenv=globalenv),
  linux_cxx("Clang 10: C++14,17", "clang++-10", packages="clang-10 libstdc++-9-dev", llvm_os="focal", llvm_ver="10", buildscript="drone", buildtype="boost", image="cppalliance/droneubuntu2004:1", environment={'B2_TOOLSET': 'clang-10', 'B2_CXXFLAGS': '-Werror', 'B2_CXXSTD': '14,17'}, globalenv=globalenv),
  linux_cxx("Clang 11: C++14,17", "clang++-11", packages="clang-11 libstdc++-9-dev", llvm_os="focal", llvm_ver="11", buildscript="drone", buildtype="boost", image="cppalliance/droneubuntu2004:1", environment={'B2_TOOLSET': 'clang-11', 'B2_CXXFLAGS': '-Werror', 'B2_CXXSTD': '14,17'}, globalenv=globalenv),
  linux_cxx("Clang 12: C++17,20", "clang++-12", packages="clang-12 libstdc++-9-dev", llvm_os="focal", llvm_ver="12", buildscript="drone", buildtype="boost", image="cppalliance/droneubuntu2004:1", environment={'B2_TOOLSET': 'clang-12', 'B2_CXXFLAGS': '-Werror', 'B2_CXXSTD': '17,20'}, globalenv=globalenv),
  linux_cxx("Clang 13: C++17,20", "clang++-13", packages="clang-13 libstdc++-10-dev", llvm_os="jammy", llvm_ver="13", buildscript="drone", buildtype="boost", image="cppalliance/droneubuntu2204:1", environment={'B2_TOOLSET': 'clang-13', 'B2_CXXFLAGS': '-Werror', 'B2_CXXSTD': '17,20'}, globalenv=globalenv),
  linux_cxx("Clang 14: C++17,20", "clang++-14", packages="clang-14 libstdc++-10-dev", llvm_os="jammy", llvm_ver="14", buildscript="drone", buildtype="boost", image="cppalliance/droneubuntu2204:1", environment={'B2_TOOLSET': 'clang-14', 'B2_CXXFLAGS': '-Werror', 'B2_CXXSTD': '17,20'}, globalenv=globalenv),

  # arm64 (unsigned char)
  linux_cxx("ARM64: Clang 12", "clang++-12", packages="clang-12 libstdc++-9-dev", llvm_os="focal", llvm_ver="12", buildscript="drone", buildtype="boost", image="cppalliance/droneubuntu2004:multiarch", environment={'B2_TOOLSET': 'clang-12', 'B2_CXXFLAGS': '-Werror', 'B2_CXXSTD': '11,14,17,20'}, arch="arm64", globalenv=globalenv),

  # s390x
  linux_cxx("S390x: GCC 11", "g++-11", packages="g++-11", buildtype="boost", buildscript="drone", image="cppalliance/droneubuntu2004:multiarch", environment={'B2_TOOLSET': 'gcc-11', 'B2_CXXFLAGS': '-Werror', 'B2_CXXSTD': '17'}, arch="s390x", globalenv=globalenv),

  # MSVC
  windows_cxx("MSVC 14.1", "", image="cppalliance/dronevs2017", buildtype="boost", buildscript="drone", environment={"B2_TOOLSET": "msvc-14.1", 'B2_CXXFLAGS': '/WX', "B2_CXXSTD": "11,14,17"},globalenv=globalenv),
  windows_cxx("MSVC 14.2: C++14,17,latest", "", image="cppalliance/dronevs2019", buildtype="boost", buildscript="drone", environment={"B2_TOOLSET": "msvc-14.2", 'B2_CXXFLAGS': '/WX', "B2_CXXSTD": "14,17,latest"},globalenv=globalenv),
  windows_cxx("MSVC 14.3: C++17,20", "", image="cppalliance/dronevs2022", buildtype="boost", buildscript="drone", environment={"B2_TOOLSET": "msvc-14.3", 'B2_CXXFLAGS': '/WX', "B2_CXXSTD": "17,20"},globalenv=globalenv),
]

# from https://github.com/boostorg/boost-ci
load("@boost_ci//ci/drone/:functions.star", "linux_cxx","windows_cxx","osx_cxx","freebsd_cxx")

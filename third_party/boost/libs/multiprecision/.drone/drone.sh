#!/bin/bash

set -ex
export TRAVIS_BUILD_DIR=$(pwd)
export DRONE_BUILD_DIR=$(pwd)
export TRAVIS_BRANCH=$DRONE_BRANCH
export VCS_COMMIT_ID=$DRONE_COMMIT
export GIT_COMMIT=$DRONE_COMMIT
export PATH=~/.local/bin:/usr/local/bin:$PATH

echo '==================================> BEFORE_INSTALL'

. .drone/before-install.sh

echo '==================================> INSTALL'

cd ..
for i in 1 2 3 4 5; do git clone -b $TRAVIS_BRANCH --depth 1 https://github.com/boostorg/boost.git boost-root && break || sleep 15; done
cd boost-root
for i in 1 2 3 4 5; do git submodule update --init tools/build && break || sleep 15; done
for i in 1 2 3 4 5; do git submodule update --init libs/config && break || sleep 15; done
for i in 1 2 3 4 5; do git submodule update --init libs/polygon && break || sleep 15; done
for i in 1 2 3 4 5; do git submodule update --init tools/boost_install && break || sleep 15; done
for i in 1 2 3 4 5; do git submodule update --init libs/headers && break || sleep 15; done
for i in 1 2 3 4 5; do git submodule update --init tools/boostdep && break || sleep 15; done
cp -r $TRAVIS_BUILD_DIR/* libs/multiprecision
for i in 1 2 3 4 5; do python tools/boostdep/depinst/depinst.py multiprecision && break || sleep 15; done
./bootstrap.sh
./b2 headers

echo '==================================> BEFORE_SCRIPT'

. $DRONE_BUILD_DIR/.drone/before-script.sh

echo '==================================> SCRIPT'

echo "using $TOOLSET : : $COMPILER : <cxxflags>-std=$CXXSTD ;" > ~/user-config.jam
(cd libs/config/test && ../../../b2 print_config_info print_math_info toolset=$TOOLSET)
(cd libs/multiprecision/test && ../../../b2 -j3 toolset=$TOOLSET $TEST_SUITE define=CI_SUPPRESS_KNOWN_ISSUES define=SLOW_COMPILER)

echo '==================================> AFTER_SUCCESS'

. $DRONE_BUILD_DIR/.drone/after-success.sh

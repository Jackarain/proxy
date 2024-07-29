#!/bin/bash

# Copyright 2022 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

set -ex
export PATH=~/.local/bin:/usr/local/bin:$PATH
uname -a
echo $DRONE_STAGE_MACHINE

DRONE_BUILD_DIR=$(pwd)

BOOST_BRANCH=develop
if [ "$DRONE_BRANCH" = "master" ]; then BOOST_BRANCH=master; fi

cd ..
git clone -b $BOOST_BRANCH --depth 1 https://github.com/boostorg/boost.git boost-root
cd boost-root
git submodule update --init tools/boostdep
mkdir -p libs/$LIBRARY
cp -r $DRONE_BUILD_DIR/* libs/$LIBRARY
python tools/boostdep/depinst/depinst.py -I example $LIBRARY
./bootstrap.sh
./b2 -d0 headers

if [[ $(uname) == "Linux" ]]; then
    echo 0 | sudo tee /proc/sys/kernel/randomize_va_space
fi

echo "using $TOOLSET : : $COMPILER ;" > ~/user-config.jam
./b2 -j3 libs/$LIBRARY/test toolset=$TOOLSET cxxstd=$CXXSTD variant=debug,release ${ADDRMD:+address-model=$ADDRMD} ${UBSAN:+undefined-sanitizer=norecover debug-symbols=on} ${ASAN:+address-sanitizer=norecover debug-symbols=on} ${CXXFLAGS:+cxxflags=$CXXFLAGS} ${CXXSTDDIALECT:+cxxstd-dialect=$CXXSTDDIALECT} ${LINKFLAGS:+linkflags=$LINKFLAGS}

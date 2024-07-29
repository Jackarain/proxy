#!/bin/bash

# Copyright 2024 Dmitry Arkhipov (grisumbras@gmail.com)
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or copy at http://boost.org/LICENSE_1_0.txt)

set -e
export TRAVIS_OS_NAME=linux
export TRAVIS_BUILD_DIR=$(pwd)
export DRONE_BUILD_DIR=$(pwd)
export DRONE_BRANCH=${DRONE_BRANCH:-$(echo $GITHUB_REF | cut -d/ -f3-)}
export TRAVIS_BRANCH=$DRONE_BRANCH
export TRAVIS_EVENT_TYPE=$DRONE_BUILD_EVENT
export VCS_COMMIT_ID=$DRONE_COMMIT
export GIT_COMMIT=$DRONE_COMMIT
export REPO_NAME=$DRONE_REPO
export USER=$(whoami)
export CC=${CC:-gcc}
export PATH=~/.local/bin:/usr/local/bin:$PATH

if [ "$DRONE_JOB_BUILDTYPE" == "test" ]; then

echo '==================================> INSTALL'

export SELF=`basename $REPO_NAME`

pwd
cd ../..
BOOST_BRANCH=develop && [ "$TRAVIS_BRANCH" == "master" ] && BOOST_BRANCH=master || true
git clone -b $BOOST_BRANCH https://github.com/boostorg/boost.git boost-root --depth 1
cd boost-root
export BOOST_ROOT=$(pwd)
git submodule update --init tools/boostbook
git submodule update --init tools/boostdep
git submodule update --init tools/build
git submodule update --init tools/quickbook
rsync -av $TRAVIS_BUILD_DIR/ tools/docca
python tools/boostdep/depinst/depinst.py ../tools/quickbook
./bootstrap.sh
./b2 headers

echo '==================================> SCRIPT'

echo "using python : : python3 ;" > tools/build/src/user-config.jam
./b2 tools/docca/test

elif [ "$DRONE_JOB_BUILDTYPE" == "example" ]; then

echo '==================================> INSTALL'

export SELF=`basename $REPO_NAME`

pwd
cd ..
mkdir -p $HOME/cache && cd $HOME/cache
if [ ! -d doxygen ]; then git clone -b 'Release_1_8_15' --depth 1 https://github.com/doxygen/doxygen.git && echo "not-cached" ; else echo "cached" ; fi
cd doxygen
cmake -H. -Bbuild -DCMAKE_BUILD_TYPE=Release
cd build
sudo make install
cd ../..

if [ ! -f saxonhe.zip ]; then
  wget -O saxonhe.zip https://sourceforge.net/projects/saxon/files/Saxon-HE/9.9/SaxonHE9-9-1-4J.zip/download && echo "not-cached"
else
  echo "cached"
fi
unzip -o saxonhe.zip
sudo rm /usr/share/java/Saxon-HE.jar
sudo cp saxon9he.jar /usr/share/java/Saxon-HE.jar

cd ..
BOOST_BRANCH=develop && [ "$TRAVIS_BRANCH" == "master" ] && BOOST_BRANCH=master || true
git clone -b $BOOST_BRANCH https://github.com/boostorg/boost.git boost-root --depth 1
cd boost-root
export BOOST_ROOT=$(pwd)
git submodule update --init tools/boostbook
git submodule update --init tools/boostdep
git submodule update --init tools/build
git submodule update --init tools/quickbook
rsync -av $TRAVIS_BUILD_DIR/ tools/docca
python tools/boostdep/depinst/depinst.py ../tools/quickbook
./bootstrap.sh
./b2 headers

echo '==================================> SCRIPT'

echo "using doxygen ; using boostbook ; using saxonhe ; using python : : python3 ;" > tools/build/src/user-config.jam
./b2 tools/docca/example

fi

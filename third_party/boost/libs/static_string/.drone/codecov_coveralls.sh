#!/bin/bash

# Copyright 2020 Rene Rivera, Sam Darwin
# Copyright 2025 Alexander Grund
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or copy at http://boost.org/LICENSE_1_0.txt)


set -xe

# Run the regular Boost CI script which does the test & upload to codecov.io
wget https://github.com/boostorg/boost-ci/raw/refs/heads/master/.drone/drone.sh
. drone.sh

# coveralls
# uses multiple lcov steps from boost-ci codecov.sh script
if [ -n "${COVERALLS_REPO_TOKEN}" ]; then
    echo "processing coveralls"
    pip3 install --user cpp-coveralls
    cd "$BOOST_CI_SRC_FOLDER"

    export PATH=/tmp/lcov/bin:$PATH
    command -v lcov
    lcov --version

    lcov --remove coverage.info -o coverage_filtered.info '*/test/*' '*/extra/*'
    cpp-coveralls --verbose -l coverage_filtered.info
fi

#!/bin/bash

# Copyright 2020 Rene Rivera, Sam Darwin
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or copy at http://boost.org/LICENSE_1_0.txt)

set -e

export USER=$(whoami)
export CC=${CC:-gcc}
export PATH=~/.local/bin:/usr/local/bin:$PATH

git clone https://github.com/boostorg/boost-ci.git boost-ci-cloned --depth 1
[ "$(basename $DRONE_REPO)" == "boost-ci" ] || cp -prf boost-ci-cloned/ci .
rm -rf boost-ci-cloned

export BOOST_CI_TARGET_BRANCH="$DRONE_BRANCH"
export BOOST_CI_SRC_FOLDER=$(pwd)
export CODECOV_NAME=${CODECOV_NAME:-"Drone CI"}

set +x
echo '==================================> INSTALL'

scripts=(
    "$BOOST_CI_SRC_FOLDER/.drone/before-install.sh"
    "$BOOST_CI_SRC_FOLDER/ci/common_install.sh"
    "$BOOST_CI_SRC_FOLDER/.drone/after-install.sh"
)
for script in "${scripts[@]}"; do
    if [ -e "$script" ]; then
        echo "==============================> RUN $script"
        source "$script"
        set +x
    fi
done

echo "B2 config: $(env | grep B2_ || true)"
echo "==================================> SCRIPT ($DRONE_JOB_BUILDTYPE)"

case "$DRONE_JOB_BUILDTYPE" in
    boost)
        $BOOST_CI_SRC_FOLDER/ci/build.sh
        ;;
    codecov)
        $BOOST_CI_SRC_FOLDER/ci/travis/codecov.sh
        ;;
    valgrind)
        $BOOST_CI_SRC_FOLDER/ci/travis/valgrind.sh
        ;;
    coverity)
        if [ -z "$COVERITY_SCAN_NOTIFICATION_EMAIL" ] || [ -z "$COVERITY_SCAN_TOKEN" ]; then
            echo "Coverity details not set up"
            [ -n "$COVERITY_SCAN_NOTIFICATION_EMAIL" ] || echo 'Missing $COVERITY_SCAN_NOTIFICATION_EMAIL'
            [ -n "$COVERITY_SCAN_TOKEN" ] || echo 'Missing $COVERITY_SCAN_TOKEN'
            exit 1
        fi
        echo "DRONE_BRANCH=$DRONE_BRANCH, DRONE_BUILD_EVENT=$DRONE_BUILD_EVENT, DRONE_REPO=$DRONE_REPO"
        if [[ "$DRONE_BRANCH" =~ ^(master|develop)$ ]] && [[ "$DRONE_BUILD_EVENT" =~ ^(push|cron)$ ]]; then
            export BOOST_REPO="$DRONE_REPO"
			export BOOST_BRANCH="$DRONE_BRANCH"
            $BOOST_CI_SRC_FOLDER/ci/coverity.sh
        fi
        ;;
    *)
        echo "Unknown build type: $DRONE_JOB_BUILDTYPE"
        ;;
esac

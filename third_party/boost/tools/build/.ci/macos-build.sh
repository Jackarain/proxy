#!/usr/bin/env bash

set -e
cd src/engine
./build.sh ${TOOLSET}
./b2 -v
cd ../..

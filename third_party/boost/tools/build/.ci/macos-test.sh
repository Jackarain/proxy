#!/usr/bin/env bash

set -e
cd test
./test_all.py ${TOOLSET}
cd ..

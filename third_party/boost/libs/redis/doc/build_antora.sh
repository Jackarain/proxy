#!/bin/bash
#
# Copyright (c) 2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

set -e

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
cd "$SCRIPT_DIR"

# Required by our CMake.
# Prevents Antora from cloning Boost again
export BOOST_SRC_DIR=$(realpath $SCRIPT_DIR/../../..)

npm ci
npx antora --log-format=pretty redis-playbook.yml

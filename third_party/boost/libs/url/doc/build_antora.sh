#
# Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/url
#

set -xe

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
cd "$SCRIPT_DIR"

if [ -z "${BOOST_SRC_DIR:-}" ]; then
  CANDIDATE=$( cd "$SCRIPT_DIR/../../.." 2>/dev/null && pwd )
  if [ -n "$CANDIDATE" ]; then
    BOOST_SRC_DIR_IS_VALID=ON
    for F in "CMakeLists.txt" "Jamroot" "boost-build.jam" "bootstrap.sh" "libs"; do
      if [ ! -e "$CANDIDATE/$F" ]; then
        BOOST_SRC_DIR_IS_VALID=OFF
        break
      fi
    done
    if [ "$BOOST_SRC_DIR_IS_VALID" = "ON" ]; then
      export BOOST_SRC_DIR="$CANDIDATE"
      echo "Using BOOST_SRC_DIR=$BOOST_SRC_DIR"
    fi
  fi
fi

if [ $# -eq 0 ]
  then
    echo "No playbook supplied, using default playbook"
    PLAYBOOK="local-playbook.yml"
  else
    PLAYBOOK=$1
fi

echo "Building documentation with Antora..."
echo "Installing npm dependencies..."
npm ci

echo "Building docs in custom dir..."
PATH="$(pwd)/node_modules/.bin:${PATH}"
export PATH
npx antora --clean --fetch "$PLAYBOOK" --stacktrace --log-level all
echo "Done"

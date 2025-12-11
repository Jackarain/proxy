#!/bin/bash

#
# Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official REPOSITORY: https://github.com/boostorg/openmethod
#

set -e

if [ $# -eq 0 ]
  then
    echo "No playbook supplied, using default playbook"
    PLAYBOOK="local-playbook.yml"
  else
    PLAYBOOK=$1
fi

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

if [ -n "${BOOST_SRC_DIR:-}" ]; then
  if [ -n "${CIRCLE_REPOSITORY_URL:-}" ]; then
    if [[ "$CIRCLE_REPOSITORY_URL" =~ boostorg/boost(\.git)?$ ]]; then
      LIB="$(basename "$(dirname "$SCRIPT_DIR")")"
      REPOSITORY="boostorg/${LIB}"
    else
      ACCOUNT="${CIRCLE_REPOSITORY_URL#*:}"
      ACCOUNT="${ACCOUNT%%/*}"
      LIB=$(basename "$(git rev-parse --show-toplevel)")
      REPOSITORY="${ACCOUNT}/${LIB}"
    fi
    SHA=$(git -C "$BOOST_SRC_DIR/libs" ls-tree HEAD | grep -w openmethod | awk '{print $3}')
  elif [ -n "${GITHUB_REPOSITORY:-}" ]; then
    REPOSITORY="${GITHUB_REPOSITORY}"
    SHA="${GITHUB_SHA}"
  fi
fi

cd "$SCRIPT_DIR"

if [ -n "${REPOSITORY}" ] && [ -n "${SHA}" ]; then
  base_url="https://github.com/${REPOSITORY}/blob/${SHA}"
  echo "Setting base-url to $base_url"
  cp mrdocs.yml mrdocs.yml.bak
  perl -i -pe 's{^\s*base-url:.*$}{base-url: '"$base_url/"'}' mrdocs.yml
else
  echo "REPOSITORY or SHA not set; skipping base-url modification"
fi

echo "Building documentation with Antora..."
echo "Installing npm dependencies..."
npm ci

echo "Building docs in custom dir..."
PATH="$(pwd)/node_modules/.bin:${PATH}"
export PATH
npx antora --clean --fetch "$PLAYBOOK" --stacktrace # --log-level all

echo "Fixing links to non-mrdocs URIs..."

for f in $(find html -name '*.html'); do
  perl -i -pe 's{&lcub;&lcub;(.*?)&rcub;&rcub;}{<a href="../../../$1.html">$1</a>}g' "$f"
done

if [ -n "${base_url:-}" ]; then
  if [ -f mrdocs.yml.bak ]; then
    mv -f mrdocs.yml.bak mrdocs.yml
    echo "Restored original mrdocs.yml"
  else
    echo "mrdocs.yml.bak not found; skipping restore"
  fi
  perl -i -pe "s[{{BASE_URL}}][$base_url]g" \
    html/openmethod/ref_headers.html html/openmethod/BOOST_OPENMETHOD*.html
fi

echo "Done"

#! /bin/sh --
# ============================================================================
#                       Copyright 2023 Alan de Freitas.
#                        Copyright 2025 Gennaro Prota.
#
#          Distributed under the Boost Software License, Version 1.0.
#              (See accompanying file LICENSE_1_0.txt or copy at
#                    http://www.boost.org/LICENSE_1_0.txt)
# ============================================================================

set -eu

if [ $# -eq 0 ]
then
    echo "No playbook supplied, using default playbook."
    PLAYBOOK="local-playbook.yml"
else
    PLAYBOOK=$1
fi

SCRIPT_DIR=$( dirname -- "${BASH_SOURCE[0]}" )
cd "$SCRIPT_DIR"

echo "Installing npm dependencies..."
npm ci

echo "Building docs in custom dir..."
PATH="$(pwd)/node_modules/.bin:${PATH}"
export PATH
npx antora --clean --fetch "$PLAYBOOK" --stacktrace --log-level all
echo "Done"

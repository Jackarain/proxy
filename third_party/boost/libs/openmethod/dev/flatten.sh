#!/bin/bash

mkdir -p build_outputs_folder/boost/openmethod

python3 dev/flatten.py build_outputs_folder/boost/openmethod.hpp \
    include/boost/openmethod.hpp

for header in core compiler shared_ptr unique_ptr; do
    python3 dev/flatten.py "build_outputs_folder/boost/openmethod/$header.hpp" \
        "include/boost/openmethod/$header.hpp"
done

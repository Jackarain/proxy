#!/usr/bin/env bash

set -e
uname -a
ls -laF /Applications
sudo xcode-select -switch ${XCODE_APP}
which clang++

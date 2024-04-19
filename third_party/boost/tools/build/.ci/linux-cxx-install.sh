#!/bin/sh

# Usage:
# LLVM_OS: LLVM OS release to obtain clang binaries. Only needed for clang install.
# LLVM_VER: The LLVM toolset version to point the repo at.
# PACKAGES: Compiler packages to install.

if command -v sudo ; then
    SUDO="sudo -E"
fi
OS_ISSUE=`cat /etc/issue | head -n1 | cut -d " " -f1`
echo ">>>>> OS Issue: ${OS_ISSUE}"

PACKAGES=${PACKAGES:-build-essential}
# APT_OPT="-o Acquire::Retries=3 -yq --no-install-suggests --no-install-recommends"
APT_OPT="-o Acquire::Retries=3 -yq"

set -e
echo ">>>>>"
echo ">>>>> APT: UPDATE 1/2.."
echo ">>>>>"
${SUDO} apt-get ${APT_OPT} update
echo ">>>>>"
echo ">>>>> APT: INSTALL 1/2: wget.."
echo ">>>>>"
${SUDO} apt-get ${APT_OPT} install wget
echo ">>>>>"
echo ">>>>> APT: REPO.."
echo ">>>>>"
${SUDO} apt-get ${APT_OPT} install software-properties-common
if test "${OS_ISSUE}" = "Ubuntu" ; then
    ${SUDO} apt-add-repository -y "ppa:ubuntu-toolchain-r/test"
fi
if test -n "${LLVM_OS}" ; then
    wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
    ${SUDO} apt-add-repository "deb http://apt.llvm.org/${LLVM_OS}/ llvm-toolchain-${LLVM_OS}-${LLVM_VER} main"
fi
echo ">>>>>"
echo ">>>>> APT: UPDATE 2/2.."
echo ">>>>>"
${SUDO} apt-get ${APT_OPT} update
echo ">>>>>"
echo ">>>>> APT: INSTALL 2/2: ${PACKAGES}.."
echo ">>>>>"
${SUDO} apt-get ${APT_OPT} install ${PACKAGES}

# Use, modification, and distribution are
# subject to the Boost Software License, Version 1.0. (See accompanying
# file LICENSE.txt)
#
# Copyright Rene Rivera 2020-2023.

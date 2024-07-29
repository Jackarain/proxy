# Use, modification, and distribution are
# subject to the Boost Software License, Version 1.0. (See accompanying
# file LICENSE.txt)
#
# Copyright 2024 Dmitry Arkhipov (grisumbras@yandex.ru)

# For Drone CI we use the Starlark scripting language to reduce duplication.
# As the yaml syntax for Drone CI is rather limited.
#
#
globalenv={'B2_CI_VERSION': '1', 'B2_FLAGS': 'warnings=extra warnings-as-errors=on'}
linuxglobalimage="cppalliance/droneubuntu1804:1"

def main(ctx):
  return [
    linux_cxx("tests", "g++", packages="docbook docbook-xml docbook-xsl python3-jinja2 python3-pytest xsltproc libsaxonhe-java default-jre-headless flex libfl-dev bison unzip rsync", buildtype="test", buildscript="drone", image=linuxglobalimage, globalenv=globalenv),
    linux_cxx("examples", "g++", packages="docbook docbook-xml docbook-xsl python3-jinja2 python3-pytest xsltproc libsaxonhe-java default-jre-headless flex libfl-dev bison unzip rsync", buildtype="example", buildscript="drone", image=linuxglobalimage, globalenv=globalenv),
  ]

# from https://github.com/boostorg/boost-ci
load("@boost_ci//ci/drone/:functions.star", "linux_cxx")

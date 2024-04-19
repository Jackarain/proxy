#!/usr/bin/env python3

# Copyright 2023 René Ferdinand Rivera Morell
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or https://www.bfgroup.xyz/b2/LICENSE.txt)

import os
import BoostBuild

t = BoostBuild.Tester(pass_toolset=False)

t.write("Jamroot", """
import regex ;
local r ;
local a = [ regex.grep . : *.*pp : "#(include) <([^>]+)>" ] ;
while $(a)
{
    r += "$(a[1]:D=) $(a[2])" ;
    a = $(a[3-]) ;
}
local b = [ regex.grep . : *.*pp : "#(include) <([^>]+)>" : 1 2 ] ;
while $(b)
{
    r += "$(b[1]:D=) $(b[2]) $(b[3])" ;
    b = $(b[4-]) ;
}
for local s in [ SORT $(r) ] { ECHO $(s) ; }
EXIT : 0 ;
""")
t.write("a.cpp", """
#include <b.hpp>
""")
t.write("b.hpp", """
#include <a>
""")

t.run_build_system()
t.expect_output_lines([
    "a.cpp #include <b.hpp>",
    "a.cpp include b.hpp",
    "b.hpp #include <a>",
    "b.hpp include a",
])
t.expect_nothing_more()

t.cleanup()

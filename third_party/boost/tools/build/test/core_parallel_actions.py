#!/usr/bin/env python3

# Copyright 2006 Rene Rivera.
# Copyright 2011 Steven Watanabe
# Copyright 2026 Paolo Pastori
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or https://www.bfgroup.xyz/b2/LICENSE.txt)

import BoostBuild
import sys

t = BoostBuild.Tester(["-d1"], pass_toolset=False)

t.write("file.jam", """\
actions sleeper
{{
    "{}" -c "import time; time.sleep($(<:B))"
}}

rule sleeper
{{
    DEPENDS $(<) : $(>) ;
}}

NOTFILE front ;
sleeper 0.2.a : front ;
sleeper 0.3.a : front ;
sleeper 0.4.a : front ;
NOTFILE choke ;
DEPENDS choke : 0.2.a 0.3.a 0.4.a ;
sleeper 0.2.b : choke ;
sleeper 0.3.b : choke ;
sleeper 0.4.b : choke ;
DEPENDS bottom : 0.2.b 0.3.b 0.4.b ;
DEPENDS all : bottom ;
""".format(sys.executable))

t.run_build_system(["-ffile.jam", "-j3"])
t.expect_output_lines("""\
...found 10 targets...
...updating 6 targets...
sleeper 0.[234].a
sleeper 0.[2-4].a
sleeper 0.[234].a
sleeper 0.[2-4].b
sleeper 0.[234].b
sleeper 0.[2-4].b

...updated 6 targets...
""")

t.cleanup()

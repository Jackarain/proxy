#!/usr/bin/env python3

# Copyright 2007 Rene Rivera.
# Copyright 2011 Steven Watanabe
# Copyright 2026 Paolo Pastori
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or https://www.bfgroup.xyz/b2/LICENSE.txt)

#   Added to guard against a bug causing targets to be used before they
# themselves have finished building. This used to happen for targets built by a
# multi-file action that got triggered by another target.
#
# Example:
#   When target A and target B were declared as created by a single action and
# target A triggered running that action then, while the action was still
# running, target B was already reporting as being built causing other targets
# depending on target A to be built prematurely.

import BoostBuild
import sys

sleep1_s = 1.0
sleep2_s = 0.1

t = BoostBuild.Tester(["-d1"], pass_toolset=False)

t.write("file.jam", """\
actions .gen.
{{
    echo 001
    "{0}" -c "import time; time.sleep({1})"
    echo 002
}}
rule .use.1 {{ DEPENDS $(<) : $(>) ; }}
actions .use.1
{{
    echo 003
}}

rule .use.2 {{ DEPENDS $(<) : $(>) ; }}
actions .use.2
{{
    "{0}" -c "import time; time.sleep({2})"
    echo 004
}}

.gen. g1.generated g2.generated ;
.use.1 u1.user : g1.generated ;
.use.2 u2.user : g2.generated ;

DEPENDS all : u1.user u2.user ;
""".format(sys.executable, sleep1_s, sleep2_s))

t.run_build_system(["-ffile.jam", "-j2"], stdout="""\
...found 5 targets...
...updating 4 targets...
.gen. g1.generated
001
002
.use.1 u1.user
003
.use.2 u2.user
004

...updated 4 targets...
""")

t.cleanup()

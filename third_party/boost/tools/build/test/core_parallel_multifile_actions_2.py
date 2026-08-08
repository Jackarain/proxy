#!/usr/bin/env python3

# Copyright 2008 Jurko Gospodnetic, Vladimir Prus
# Copyright 2011 Steven Watanabe
# Copyright 2026 Paolo Pastori
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or https://www.bfgroup.xyz/b2/LICENSE.txt)

#   Added to guard against a bug causing targets to be used before they
# themselves have finished building. This used to happen for targets built by a
# multi-file action that got triggered by another target, except when the
# target triggering the action was the first one in the list of targets
# produced by that action.
#
# Example:
#   When target A and target B were declared as created by a single action with
# A being the first one listed, and target B triggered running that action
# then, while the action was still running, target A was already reporting as
# being built causing other targets depending on target A to be built
# prematurely.

import BoostBuild
import sys

sleep_s = 0.2

t = BoostBuild.Tester(pass_toolset=False)

t.write("file.jam", """\
actions link
{{
    "{}" -c "import time; time.sleep({})"
    echo 001 - linked
}}

link dll lib ;

actions install
{{
    echo 002 - installed
}}

install installed_dll : dll ;
DEPENDS installed_dll : dll ;

DEPENDS all : lib installed_dll ;
""".format(sys.executable, sleep_s))

t.run_build_system(["-ffile.jam", "-j2"], stdout="""\
...found 4 targets...
...updating 3 targets...
link dll
001 - linked
install installed_dll
002 - installed

...updated 3 targets...
""")

t.cleanup()

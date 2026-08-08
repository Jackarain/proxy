#!/usr/bin/env python3

# Copyright 2007 Rene Rivera.
# Copyright 2011 Steven Watanabe
# Copyright 2026 Paolo Pastori
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or copy at
# https://www.bfgroup.xyz/b2/LICENSE.txt)

import BoostBuild
import sys

sleep_s = 1.5

t = BoostBuild.Tester(pass_toolset=False)

t.write("file.jam", """\
actions .a. {{
"{}" -c "import time; time.sleep({})"
}}

.a. sleeper ;

DEPENDS all : sleeper ;
""".format(sys.executable, sleep_s))

t.run_build_system(["-ffile.jam", "-l1"], status=1)
t.expect_output_lines("1 second time limit exceeded")

t.cleanup()

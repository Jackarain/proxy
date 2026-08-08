#!/usr/bin/env python3

# Copyright 2026 Paolo Pastori
# Copyright 2026 Rene Ferdinand Rivera Morell
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or https://www.bfgroup.xyz/b2/LICENSE.txt)

# Test for the JAM_SEMAPHORE variable:
# verify that actions for targets sharing the same named semaphore
# cannot be run in parallel.

import sys


def touch(fname):
    with open(fname, "w"):
        pass


def update(mode, sentry, secs, target):
    from os.path import isfile
    from time import sleep
    from os import unlink

    secs = float(secs)
    other = {"x1": "x2", "x2": "x1"}
    # Indicate we started.
    touch(sentry + target)
    # Check for parallel action to start.
    while not isfile(sentry + other[target]) and secs > 0:
        sleep(0.05)
        secs -= 0.05
    if mode == "parallel":
        # Did not see the other action start in time.
        if secs <= 0:
            raise TimeoutError
        # Both actions are now running in parallel.
        print("PARALLEL UPDATE")
        sleep(secs)
    else:
        # For semaphore mode, having the sentry check not time out means the
        # parallel action ran, ignoring the semaphore.
        if secs > 0:
            raise RuntimeError
    # Create our result target and indicate we completed.
    touch(target)
    unlink(sentry + target)


if len(sys.argv) == 5:
    # called by jam update action
    # python semaphore.py <mode> <sentry_prefix> <sleep_secs> <target_fname>
    update(*sys.argv[1:])
    sys.exit()

import os.path

# remember this script absolute pathname
script = os.path.abspath(__file__)

import BoostBuild

t = BoostBuild.Tester(["-ffile.jam", "-j2"], pass_toolset=False)

# 1. test parallel execution of update
t.write(
    "file.jam",
    """\
actions update
{{
    "{}" "{}" parallel sentry 1 $(<)
}}
update x1 ;
update x2 ;
DEPENDS all : x1 x2 ;
""".format(
        sys.executable, script
    )
)

t.run_build_system()
t.expect_addition("x1")
t.expect_addition("x2")
t.expect_output_lines("PARALLEL UPDATE")

t.rm("x1")
t.rm("x2")

# 2. test parallel execution suppression by JAM_SEMAPHORE
t.write(
    "file.jam",
    """\
actions update
{{
    "{}" "{}" semaphore sentry 1 $(<)
}}
JAM_SEMAPHORE on x1 x2 = <s>update_sem ;
update x1 ;
update x2 ;
DEPENDS all : x1 x2 ;
""".format(
        sys.executable, script
    )
)

expected_output = """\
...found 3 targets...
...updating 2 targets...
update x1
update x2

...updated 2 targets...
"""

t.run_build_system(stdout=expected_output)
t.expect_addition("x1")
t.expect_addition("x2")

t.cleanup()

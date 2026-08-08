#!/usr/bin/env python3

# Copyright 2026 Paolo Pastori
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or https://www.bfgroup.xyz/b2/LICENSE.txt)

# Test for --durations option

import BoostBuild

t = BoostBuild.Tester(pass_d0=True)

t.write('foo.cpp', '')
t.write('Jamroot', "obj foo : foo.cpp ;\n")

t.run_build_system(['--durations=1'], status=0)
t.expect_output_lines("============================= slowest 1 durations *")
t.expect_output_lines("[0-9].[0-9][0-9][0-9]s *")

t.cleanup()

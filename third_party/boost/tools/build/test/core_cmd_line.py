#!/usr/bin/env python3

# Copyright 2026 Paolo Pastori
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or https://www.bfgroup.xyz/b2/LICENSE.txt)

# Test for command line argument processing

import BoostBuild

t = BoostBuild.Tester()

# 1. test error on non-existent target
t.write('Jamroot', '')

t.run_build_system(['foo', '-d0'], status=1, stdout="""\
notice: could not find main target foo
notice: assuming it is a name of file to create.
don't know how to make <e>foo
""")

# 2. test skipping of non-existent options, i.e. Lyra relaxed parsing mode
t.run_build_system(['-°', '--°°°'], status=0, stdout="...found 1 target...\n")

# 3. test POSIX conformance to option value allowed separators (see
# Lyra option_style::posix()), this is to check for a bug in Jam command
# line processing of option value separated by space (see #562)
t.run_build_system(['-d0', '--jobs=2'], status=0, stdout='')
t.run_build_system(['-d0', '--jobs', '2'], status=0, stdout='')

# 4. test for errors on missing option value
t.run_build_system(['-j', '-n'], status=1)
t.expect_output_lines("error: Invalid value for the '-j' option.")

t.expect_nothing_more()

t.cleanup()

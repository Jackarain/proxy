#!/usr/bin/env python3

# Copyright 2026 Paolo Pastori
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or https://www.bfgroup.xyz/b2/LICENSE.txt)

# List of tests. Each tuple contains the value used for the test,
# the pattern, the replacement, and the expected result (do not use [] here.)
# Any test is processed by an instruction such as
#
# ECHO [ SUBST <value> <pattern> <replacement> ] ;
#
# fell free to add more test cases...

# remember to use raw strings to avoid surprises with escaping
trials = [
    (r'""', r'""', r'""', r''),
    # no matching value
    (r'aeio', r'u', r'xyz', r''),
    # matched value
    (r'aeio', r'i', r'xyz', r'xyz'),
    # \\0 placeholder in replacement
    (r'aeio', r'i', r'\\0', r'i'),
    # placeholders in replacement
    (r'"234.15 $"', r'"([0-9.]+) (.)"', r'"currency: $2, amount: \\1"', r'currency: $, amount: 234.15'),
    # special chars in replacement
    (r'"total: 100"', r'"total: ([0-9]+)"', r'"$$1"', r'$100'), # NOTE: $$ in replacement does not eat $
    (r'"total: 3"', r'"^total: ([0-9]+)$"', r'"$\\1"', r'$3'), # NOTE: $\ in replacement does not eat \
    (r'/etc/doc', r'^/etc/(.+)$', r'"C:\\$1"', r'C:\doc'), # NOTE: \$ in replacement does not eat $
    (r'/etc/doc', r'^/etc/(.+)$', r'"C:\\\\1"', r'C:\doc'), # NOTE: \\ in replacement does not eat \
]

# Do not change code below !

testln = []
exptln = []
for n, c in enumerate(trials):
    testln.append('ECHO {} [ SUBST {} {} {} ] ;'.format(n, *c))
    exptln.append('{} {}'.format(n, c[3]) if c[3] else str(n))
testln.append('EXIT : 0 ;\n')
exptln.append('\n')

import BoostBuild

t = BoostBuild.Tester(pass_toolset=False)

t.run_build_system(
    ["-d0", "-f-"],
    stdin='\n'.join(testln),
    stdout='\n'.join(exptln)
)

t.cleanup()

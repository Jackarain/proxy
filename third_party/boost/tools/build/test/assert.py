#!/usr/bin/env python3

# Copyright 2023 Nikita Kniazev
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or copy at
# https://www.bfgroup.xyz/b2/LICENSE.txt)

# Tests assert module

import BoostBuild

t = BoostBuild.Tester(pass_toolset=False)

t.write("Jamroot", """\
import assert ;

rule foo ( ) { return 1 2 3 ; }

assert.result 1 2 4 : foo ;
""")

t.run_build_system(status=1, stdout="""\
error: assertion failure: [ foo ]
error: Expected: [ "1" "2" "4" ]
error: Got: [ "1" "2" "3" ]
""", match_filter=lambda x: "from module" not in x)
t.expect_nothing_more()

t.cleanup()

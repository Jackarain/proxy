#!/usr/bin/env python3

# Copyright 2025 Dmitry Arkhipov <grisumras@yandex.ru>
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or https://www.bfgroup.xyz/b2/LICENSE.txt)

# Test that subprojects of a project with rooted id can be resolved.

import BoostBuild

t = BoostBuild.Tester(use_test_config=False)

t.write("jamroot.jam", """
project /A ;
alias x ;
alias y : /A/B/C//c ;
alias z : /A/B//b ;
""")

t.write("B/build.jam", "alias b ;")

t.write("B/C/build.jam", "alias c ;")

t.run_build_system()

t.cleanup()

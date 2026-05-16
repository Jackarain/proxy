#!/usr/bin/env python3

# Copyright Ivan Kotov 2025.
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or copy at
# https://www.bfgroup.xyz/b2/LICENSE.txt)

# Regression test. When Jamfile contained "using whatever ; " and the 'whatever'
# module declared a project, then all targets in Jamfile were considered to be
# declared in the project associated with 'whatever', not with the Jamfile.

import BoostBuild

t = BoostBuild.Tester(use_test_config=False)

t.write("jamroot.jam", """\

rule test {
    value = zero ;
    echo $(value) ;
    echo $$(value) ;
    echo $$$(value) ;
    echo $$$$(value) ;
    echo $$$$ ;
}

test a.test ;

""")

t.run_build_system(stdout="""zero
$(value)
$zero
$$(value)
$$$$
...found 1 target...
""")

t.cleanup()

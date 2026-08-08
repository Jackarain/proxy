#!/usr/bin/env python3

# Copyright 2003 Vladimir Prus
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or https://www.bfgroup.xyz/b2/LICENSE.txt)

# This tests the typechecking facilities.

import BoostBuild

t = BoostBuild.Tester(["-ffile.jam"], pass_toolset=False)

t.write("file.jam", """
module .typecheck
{
    rule "[path]" ( x )
    {
        if ! [ MATCH "^(::)" : $(x) ]
        {
            ECHO "Error: $(x) is not a path" ;
            return true ;
        }
    }
}

rule do ( [path] a )
{
}

do $(ARGUMENT) ;

actions dummy { }
dummy all ;
""")

t.run_build_system(["-sARGUMENT=::a/b/c"])
t.run_build_system(["-sARGUMENT=a/b/c"], status=1, stdout="""\
Error: a/b/c is not a path
file.jam:18: in module scope
error: rule do ( [path] a )
error: called with: ( a/b/c )
error: true a
""")

t.cleanup()

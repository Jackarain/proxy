#!/usr/bin/env python3

# Copyright 2026 Paolo Pastori
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or https://www.bfgroup.xyz/b2/LICENSE.txt)

# Test for --out-xml option

import BoostBuild

t = BoostBuild.Tester(pass_d0=True)

t.write('foo.cpp', '')
t.write('Jamroot', "obj foo : foo.cpp ;\n")

t.run_build_system(['--out-xml=foo.xml'], status=0)

t.expect_addition('bin/*')
t.expect_addition('foo.xml')
t.expect_content_lines('foo.xml', [
    # NOTE: very peculiar escaping is needed here
    '<[?]xml version="1.0" encoding="utf-8"[?]>',
    '      <source><![[]CDATA[[]<p.>foo.cpp]]></source>',
])

t.expect_nothing_more()

t.cleanup()

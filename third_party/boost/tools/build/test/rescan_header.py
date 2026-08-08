#!/usr/bin/env python3

# Copyright 2012 Steven Watanabe
# Copyright 2026 Paolo Pastori
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or https://www.bfgroup.xyz/b2/LICENSE.txt)

import BoostBuild
import sys

sleep_s = 0.2

t = BoostBuild.Tester(["-j2"])

# Test a header loop that depends on (but does not contain) a generated header.
t.write("test.cpp", '#include "header1.h"\n')

t.write("header1.h", """\
#ifndef HEADER1_H
#define HEADER1_H
#include "header2.h"
#endif
""")

t.write("header2.h", """\
#ifndef HEADER2_H
#define HEADER2_H
#include "header1.h"
#include "header3.h"
#endif
""")

t.write("header3.in", "/* empty file */\n")

t.write("jamroot.jam", """\
import common ;
make header3.h : header3.in : @common.copy ;
obj test : test.cpp : <implicit-dependency>header3.h ;
""")

t.run_build_system()
t.expect_addition("bin/header3.h")
t.expect_addition("bin/$toolset/debug*/test.obj")
t.expect_nothing_more()

t.rm(".")

# Test a linear sequence of generated headers.
t.write("test.cpp", '#include "header1.h"\n')

t.write("header1.in", """\
#ifndef HEADER1_H
#define HEADER1_H
#include "header2.h"
#endif
""")

t.write("header2.in", """\
#ifndef HEADER2_H
#define HEADER2_H
#include "header3.h"
#endif
""")

t.write("header3.in", "/* empty file */\n")

t.write("jamroot.jam", """\
import common ;
make header1.h : header1.in : @common.copy ;
make header2.h : header2.in : @common.copy ;
make header3.h : header3.in : @common.copy ;
obj test : test.cpp :
    <implicit-dependency>header1.h
    <implicit-dependency>header2.h
    <implicit-dependency>header3.h ;
""")

t.run_build_system(["test"])
t.expect_addition("bin/header1.h")
t.expect_addition("bin/header2.h")
t.expect_addition("bin/header3.h")
t.expect_addition("bin/$toolset/debug*/test.obj")
t.expect_nothing_more()

t.rm(".")

# Test a loop in generated headers.
t.write("test.cpp", '#include "header1.h"\n')

t.write("header1.in", """\
#ifndef HEADER1_H
#define HEADER1_H
#include "header2.h"
#endif
""")

t.write("header2.in", """\
#ifndef HEADER2_H
#define HEADER2_H
#include "header3.h"
#endif
""")

t.write("header3.in", """\
#ifndef HEADER2_H
#define HEADER2_H
#include "header1.h"
#endif
""")

t.write("jamroot.jam", """\
import common ;

make header1.h : header1.in : @common.copy ;
make header2.h : header2.in : @common.copy ;
make header3.h : header3.in : @common.copy ;
obj test : test.cpp :
    <implicit-dependency>header1.h
    <implicit-dependency>header2.h
    <implicit-dependency>header3.h ;
""")

t.run_build_system(["test"])
t.expect_addition("bin/header1.h")
t.expect_addition("bin/header2.h")
t.expect_addition("bin/header3.h")
t.expect_addition("bin/$toolset/debug*/test.obj")
t.expect_nothing_more()

t.rm(".")

# Test that all the dependencies of a loop are updated before any of the
# dependents.
t.write("test1.cpp", '#include "header1.h"\n')

t.write("test2.cpp", """\
#include "header2.h"
int main() {}
""")

t.write("header1.h", """\
#ifndef HEADER1_H
#define HEADER1_H
#include "header2.h"
#endif
""")

t.write("header2.h", """\
#ifndef HEADER2_H
#define HEADER2_H
#include "header1.h"
#include "header3.h"
#endif
""")

t.write("header3.in", "\n")

t.write("jamroot.jam", """\
import common ;

rule copy {{ common.copy $(<) : $(>) ; }}
actions copy {{ "{}" -c "import time; time.sleep({})" }}

make header3.h : header3.in : @copy ;
exe test : test2.cpp test1.cpp : <implicit-dependency>header3.h ;
""".format(sys.executable, sleep_s))

t.run_build_system(["test"])
t.expect_addition("bin/header3.h")
t.expect_addition("bin/$toolset/debug*/test1.obj")
t.expect_addition("bin/$toolset/debug*/test2.obj")
t.expect_addition("bin/$toolset/debug*/test.exe")
t.ignore_addition("bin/*/test.rsp")
t.expect_nothing_more()

t.touch("header3.in")
t.run_build_system(["test"])
t.expect_touch("bin/header3.h")
t.expect_touch("bin/$toolset/debug*/test1.obj")
t.expect_touch("bin/$toolset/debug*/test2.obj")
t.expect_touch("bin/$toolset/debug*/test.exe")
t.ignore_touch("bin/*/test.rsp")
t.expect_nothing_more()

t.rm(".")

# Test a loop that includes a generated header
t.write("test1.cpp", '#include "header1.h"\n')
t.write("test2.cpp", """\
#include "header2.h"
int main() {}
""")

t.write("header1.h", """\
#ifndef HEADER1_H
#define HEADER1_H
#include "header2.h"
#endif
""")

t.write("header2.in", """\
#ifndef HEADER2_H
#define HEADER2_H
#include "header3.h"
#endif
""")

t.write("header3.h", """\
#ifndef HEADER3_H
#define HEADER3_H
#include "header1.h"
#endif
""")

t.write("jamroot.jam", """\
import common ;

rule copy {{ common.copy $(<) : $(>) ; }}
actions copy {{ "{}" -c "import time; time.sleep({})" }}

make header2.h : header2.in : @copy ;
exe test : test2.cpp test1.cpp : <implicit-dependency>header2.h <include>. ;
""".format(sys.executable, sleep_s))

t.run_build_system(["test"])
t.expect_addition("bin/header2.h")
t.expect_addition("bin/$toolset/debug*/test1.obj")
t.expect_addition("bin/$toolset/debug*/test2.obj")
t.expect_addition("bin/$toolset/debug*/test.exe")
t.ignore_addition("bin/*/test.rsp")
t.expect_nothing_more()

t.cleanup()

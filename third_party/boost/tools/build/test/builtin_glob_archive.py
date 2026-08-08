#!/usr/bin/env python3

# Copyright 2014 Steven Watanabe
# Copyright 2015 Artur Shepilko
# Copyright 2026 Paolo Pastori
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or https://www.bfgroup.xyz/b2/LICENSE.txt)

# This tests the GLOB_ARCHIVE rule.

import os
import sys

import BoostBuild

vms = ( os.name == 'posix' and sys.platform == 'OpenVMS')

t = BoostBuild.Tester()

## Setup test archive sources and symbols they contain.
sources = {
    "a.cpp" : ["a"],
    "b.cpp" : ["b"],
    "b_match.cpp" : ["b_match"],
    "c/nopath_check.cpp" : ["nopath_check"],
    "CaseCheck.cpp" : ["CaseCheck"],
    "seq_check1.cpp" : ["seq_check1"],
    "seq_check2.cpp" : ["seq_check2"],
    "seq_check3.cpp" : ["seq_check3"],
    "symbols_check.c" : ["symbol", "symbol_match"],
    "members_and_symbols_check.c" : ["member_and_symbol_match"],
    "symbol_case_check.c" : ["SymbolCaseCheck"],
    "main_check.cpp" : ["main"]
}


def create_sources(path, sources):
    for s in sources :
        f = os.path.join(path, s)
        t.write(f,
                "".join(["int {}() {{ return 0; }}\n".format(sym)
                         for sym in sources[s]]))


def setup_archive(name, sources):
    archive = t.adjust_names(name)[0]
    obj_suffix = t.adjust_names(".obj")[0]
    t.write("jamroot.jam","")
    ## sort the sources, so we can test order of the globbed members
    jl = [ "static-lib {} :".format(name.split(".")[0]) ]
    jl.extend(sorted(sources))
    jl.append(";\n")
    t.write("lib/jamfile.jam", "\n".join(jl))
    create_sources("lib", sources)
    t.run_build_system(subdir="lib")
    built_archive = "lib/bin/$toolset/debug*/" + name
    t.expect_addition(built_archive)
    t.copy(built_archive, name)
    t.rm("lib")
    return archive, obj_suffix


def test_glob_archive(archives, glob, expected, sort_results = False):
    ## replace placeholders
    glob = glob.replace("$archive1", archives[0]).replace("$obj", obj_suffix)
    expected = [ m.replace("$archive1",
               archives[0]).replace("$obj", obj_suffix) for m in expected ]
    if len(archives) > 1 :
        glob = glob.replace("$archive2", archives[1]).replace("$obj", obj_suffix)
        expected = [ m.replace("$archive2",
                   archives[1]).replace("$obj", obj_suffix) for m in expected ]
    ## create test jamfile
    if sort_results : glob = "[ SORT {} ]".format(glob)
    t.write("file.jam", """\
for local p in {}
{{
    ECHO $(p) ;
}}
UPDATE ;
""".format(glob))
    ## run test jamfile and match against expected results
    if sort_results : expected.sort()
    t.run_build_system(["-ffile.jam"],
                       stdout="\n".join(expected + [""]))
    t.rm("file.jam")


## RUN TESTS
archive1, obj_suffix = setup_archive("auxilliary1.lib", sources)
archive2, obj_suffix = setup_archive("auxilliary2.lib", sources)

## all arguments empty
test_glob_archive([archive1], "[ GLOB_ARCHIVE ]", [])

## empty query
test_glob_archive([archive1], "[ GLOB_ARCHIVE $archive1 : ]", [])

## no-match
test_glob_archive([archive1], "[ GLOB_ARCHIVE $archive1 : a ]", [])

## match exact
test_glob_archive([archive1], "[ GLOB_ARCHIVE $archive1 : a$obj ]",
                  ["$archive1(a$obj)"])

## glob wildcards:1
test_glob_archive([archive1], "[ GLOB_ARCHIVE $archive1 : b.* ]",
                  ["$archive1(b$obj)"])

## glob wildcards:2
test_glob_archive([archive1], '[ GLOB_ARCHIVE $archive1 : "\\b?match[.]*" ]',
                  ["$archive1(b_match$obj)"])

## glob wildcards:3
test_glob_archive([archive1], "[ SORT [ GLOB_ARCHIVE $archive1 : b* ] ]",
                  ["$archive1(b$obj)", "$archive1(b_match$obj)"])

## glob multiple patterns with multiple results.
test_glob_archive([archive1], "[ SORT [ GLOB_ARCHIVE $archive1 : b.* b_* ] ]",
                  ["$archive1(b$obj)", "$archive1(b_match$obj)"])

## glob multiple archives and patterns.
test_glob_archive([archive1, archive2],
                  "[ SORT [ GLOB_ARCHIVE $archive1 $archive2 : b.* b_* ] ]",
                  ["$archive1(b$obj)", "$archive1(b_match$obj)",
                   "$archive2(b$obj)", "$archive2(b_match$obj)"])

## glob same archive multiple times.
test_glob_archive([archive1, archive1],
                  "[ GLOB_ARCHIVE $archive1 $archive2 $archive1 : b.* ]",
                   ["$archive1(b$obj)", "$archive2(b$obj)", "$archive1(b$obj)"])

## returned archive member has no path, even though its source object-file did.
## this is rather NT-specific, where members also store their object-file's path.
test_glob_archive([archive1], "[ GLOB_ARCHIVE $archive1 : nopath_check$obj ]",
                  ["$archive1(nopath_check$obj)"])

## case insensitive matching, when archives support case sensitive member names.
## VMS implementation forces case-insensitive matching and downcased member names.

case_sensitive_members = ( not vms )

if case_sensitive_members:
    test_glob_archive([archive1],
                      "[ GLOB_ARCHIVE $archive1 : casecheck$obj : true ]",
                      ["$archive1(CaseCheck$obj)"])
elif vms:
    test_glob_archive([archive1],
                      "[ GLOB_ARCHIVE $archive1 : CaseCheck$obj : false ]",
                      ["$archive1(casecheck$obj)"])


## test the order of matched members, in general it should match the
## insertion sequence.
test_glob_archive([archive1], "[ SORT [ GLOB_ARCHIVE $archive1 : seq_check*$obj ] ]",
                  ["$archive1(seq_check1$obj)", "$archive1(seq_check2$obj)",
                   "$archive1(seq_check3$obj)"])


## glob members by symbols they contain.
## Currently supported only on VMS.
symbol_glob_supported = ( vms )

if symbol_glob_supported :
    ## NOTE: generated symbols are compiler-dependent and may be specifically
    ## mangled (as in C++ case), so globbing by exact symbol is non-trivial.
    ## However, C-generated symbols are likely to have more portable names,
    ## so for the glob-by-symbol tests we glob C-generated archive members.

    ## glob members by exact symbol.
    test_glob_archive([archive1],
                      "[ GLOB_ARCHIVE $archive1 : : : symbol ]",
                      ["$archive1(symbols_check$obj)"])

    ## glob members by symbol wildcard.
    test_glob_archive([archive1],
                      "[ GLOB_ARCHIVE $archive1 : : : symbol_* ]",
                      ["$archive1(symbols_check$obj)"])

    ## glob members by member pattern AND symbol pattern.
    test_glob_archive([archive1],
                      "[ GLOB_ARCHIVE $archive1 : *symbol* : : *member* ]",
                      ["$archive1(members_and_symbols_check$obj)"])

    ## case insensitive symbol glob.
    test_glob_archive([archive1],
                      "[ GLOB_ARCHIVE $archive1 : : true : symbolcasecheck ]",
                      ["$archive1(symbol_case_check$obj)"])

    ## glob member that contains main symbol.
    test_glob_archive([archive1],
                      "[ GLOB_ARCHIVE $archive1 : : : main _main ]",
                      ["$archive1(main_check$obj)"])

else:
    test_glob_archive([archive1],
                      "[ GLOB_ARCHIVE $archive1 : : : symbol ]",
                      [])


t.cleanup()


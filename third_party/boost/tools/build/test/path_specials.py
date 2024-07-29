#!/usr/bin/env python3

# Copyright 2012 Steven Watanabe
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or https://www.bfgroup.xyz/b2/LICENSE.txt)

# Test that paths containing spaces, unicode, and very long paths
# are handled correctly by actions.

import BoostBuild
import os
import os.path
import sys
import tempfile
from unittest.mock import patch


def test_dir(dir_name):
    with BoostBuild.Tester(use_test_config=False) as t:
        do_compile_test = True
        tmpdir = t.workpath(dir_name)
        tmp = {'TMP': tmpdir, 'TMPDIR': tmpdir}
        # cmd.exe hangs in a busy loop when TMP is a long path
        if os.name == 'nt' and len(tmpdir) > 256:
            tmp = {}
            # cl.exe and link.exe still does not support long paths
            if t.toolset.startswith('msvc'):
                do_compile_test = False
        # on windows gcc doesn't support long path, ld doesn't support neither unicode nor long path
        if os.environ.get('MSYSTEM') in ['UCRT64', 'MINGW64', 'MINGW32'] and t.toolset in ['gcc', 'clang']:
            do_compile_test = False

        t.write(dir_name + '/jamroot.jam', '''\
import testing ;
actions write-file
{
    @(STDOUT:E=okay) >"$(<)"
}
make test.txt : : @write-file ;
''' + ('''\
unit-test test : test.cpp ;
''' if do_compile_test else ''))
        t.write(dir_name + '/test.cpp', 'int main() {}\n')

        with patch.dict(os.environ, tmp):
            t.run_build_system([dir_name])
            t.expect_addition(dir_name + '/bin/test.txt')
            if do_compile_test:
                t.expect_addition(dir_name + '/bin/$toolset/debug*/test.passed')


test_dir('has space')
# Windows versions are a huge mess, like in any other Microsoft product:
# Windows 10, version 1903 codename 19H1, build number 10.0.18362
# Windows 10, version 1607 codename Redstone 1, build number 10.0.14393
if not hasattr(sys, 'getwindowsversion') or sys.getwindowsversion()[:3] >= (10, 0, 18362):
    test_dir('uni\u2665code')
if not hasattr(sys, 'getwindowsversion') or sys.getwindowsversion()[:3] >= (10, 0, 14393):
    long_path = '/'.join(['a_very_very_long_path'] * (260 // 21 + 1))
    try:
        with tempfile.TemporaryDirectory() as path:
            os.makedirs(os.path.join(path, long_path))
    except WindowsError as e:
        if e.winerror != 206:  # ERROR_FILENAME_EXCED_RANGE
            raise
    else:
        test_dir(long_path)

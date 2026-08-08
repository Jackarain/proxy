#!/usr/bin/env python3

# Copy-paste-modify from libzstd.py
# Copyright (C) 2013 Steven Watanabe
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or copy at
# https://www.bfgroup.xyz/b2/LICENSE.txt)

import BoostBuild
import MockToolset

t = BoostBuild.Tester(arguments=['toolset=mock', '--ignore-site-config', '--user-config='], pass_toolset=0)

MockToolset.create(t)

# Generic definitions that aren't configuration specific
common_stuff = '''
source_file('test.cpp', 'test.cpp')
source_file('main.cpp', 'int main() {}')
source_file('brotli/decode.h.cpp', '#include <brotli/decode.h>\\n')
action('-c -x c++ $main.cpp -o $main.o')
'''
t.write('test.cpp', 'test.cpp')

for lib in ('brotlicommon', 'brotlidec', 'brotlienc'):

    # Default initialization - static library
    t.rm('bin')
    t.write("Jamroot.jam", """
path-constant here : . ;
using brotli ;
exe test : test.cpp /brotli//%(lib)s : : <link>static <link>shared ;
""" % {'lib': lib})

    MockToolset.set_expected(t, common_stuff + '''
action('$main.o --static-lib=%(lib)s -o $config.exe')
action('-c -x c++ $brotli/decode.h.cpp -o $brotli/decode.h.o')
action('-c -x c++ $test.cpp -o $test.o')
action('$test.o --static-lib=%(lib)s -o $test')
''' % {'lib': lib})
    t.run_build_system()
    t.expect_addition('bin/mock/debug/test.exe')
    t.expect_addition('bin/mock/debug/link-static/test.exe')

    # Default initialization - shared library
    t.rm('bin')
    t.write("Jamroot.jam", """
path-constant here : . ;
using brotli ;
exe test : test.cpp /brotli//%(lib)s : : <link>static <link>shared ;
""" % {'lib': lib})

    MockToolset.set_expected(t, common_stuff + '''
action('$main.o --shared-lib=%(lib)s -o $config.exe')
action('-c -x c++ $brotli/decode.h.cpp -o $brotli/decode.h.o')
action('-c -x c++ $test.cpp -o $test.o')
action('$test.o --shared-lib=%(lib)s -o $test')
''' % {'lib': lib})
    t.run_build_system()
    t.expect_addition('bin/mock/debug/test.exe')
    t.expect_addition('bin/mock/debug/link-static/test.exe')

    # Initialization in explicit location - static library
    t.rm('bin')
    t.write("Jamroot.jam", """
path-constant here : . ;
using brotli : : <%(lib)s-name>my%(lib)s <include>$(here)/brotli <search>$(here)/brotli ;
exe test : test.cpp /brotli//%(lib)s : : <link>static <link>shared ;
""" % {'lib': lib})

    t.write('brotli/brotli/decode.h', 'brotli')

    MockToolset.set_expected(t, common_stuff + '''
action('$main.o -L./brotli --static-lib=my%(lib)s -o $config.exe')
action('-c -x c++ $test.cpp -I./brotli -o $test.o')
action('$test.o -L./brotli --static-lib=my%(lib)s -o $test')
''' % {'lib': lib})
    t.run_build_system()
    t.expect_addition('bin/mock/debug/test.exe')
    t.expect_addition('bin/mock/debug/link-static/test.exe')

    # Initialization in explicit location - shared library
    t.rm('bin')
    t.write("Jamroot.jam", """
path-constant here : . ;
using brotli : : <%(lib)s-name>my%(lib)s <include>$(here)/brotli <search>$(here)/brotli ;
exe test : test.cpp /brotli//%(lib)s : : <link>static <link>shared ;
""" % {'lib': lib})

    MockToolset.set_expected(t, common_stuff + '''
action('$main.o -L./brotli --shared-lib=my%(lib)s -o $config.exe')
action('-c -x c++ $test.cpp -I./brotli -o $test.o')
action('$test.o -L./brotli --shared-lib=my%(lib)s -o $test')
''' % {'lib': lib})
    t.run_build_system()
    t.expect_addition('bin/mock/debug/test.exe')
    t.expect_addition('bin/mock/debug/link-static/test.exe')

    # Initialization in explicit location - both static and shared libraries
    t.rm('bin')
    t.write("Jamroot.jam", """
path-constant here : . ;
using brotli : : <%(lib)s-name>my%(lib)s <include>$(here)/brotli <search>$(here)/brotli ;
exe test : test.cpp /brotli//%(lib)s
  : <link>shared:<define>SHARED : <link>static <link>shared ;
""" % {'lib': lib})

    MockToolset.set_expected(t, common_stuff + '''
action('$main.o -L./brotli --static-lib=my%(lib)s -o $config.exe')
action('$main.o -L./brotli --shared-lib=my%(lib)s -o $config.exe')
action('-c -x c++ $test.cpp -I./brotli -o $test-static.o')
action('-c -x c++ $test.cpp -I./brotli -DSHARED -o $test-shared.o')
action('$test-static.o -L./brotli --static-lib=my%(lib)s -o $test')
action('$test-shared.o -L./brotli --shared-lib=my%(lib)s -o $test')
''' % {'lib': lib})
    t.run_build_system()
    t.expect_addition('bin/mock/debug/test.exe')
    t.expect_addition('bin/mock/debug/link-static/test.exe')

t.cleanup()

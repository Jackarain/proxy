#!/usr/bin/env python3

# Copyright 2026 Paolo Pastori
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.txt or https://www.bfgroup.xyz/b2/LICENSE.txt)

# List of tested regexps. Each tuple contains the pattern,
# the value used for the test, and the expected result (do not use [] here.)
# Any test is processed by an instruction such as
#
# ECHO [ MATCH <pattern> : <value> ] ;
#
# fell free to add more test cases...

# remember to use raw strings to avoid surprises with escaping
trials = [
    (r'', r'', r''),
    # nothing captured (no parentheses in the pattern)
    (r'"hello world"', r'"hello world"', r''),
    # captures (more than one)
    (r'(first(second))(third)', r'firstsecondthird', r'firstsecond second third'),
    # ^ matches at start of line
    (r'^(world)', r'"hello world"', r''),
    (r'(world)', r'"hello world"', r'world'),
    # $ matches at end of line
    (r'(hello)$', r'"hello world"', r''),
    # . matches any single character
    (r'(.)', r'x', r'x'),
    # literal dot (with escaping looses special meaning)
    # NOTE: a \ followed by one of the characters ^.[$()|*+?\
    # matches that character taken as an ordinary character,
    # while a \ followed by any other character (but <>) does nothing!
    # NOTE: because of common escaping by shell/interpreters
    # to obtain a final \ you often have to escape itself using
    # \\ or enclose it in raw strings (Python r'..', C++ R"...")
    (r'(\\.)', r'y', r''),
    (r'(\\.)', r'.', r'.'),
    # ? matches an optional atom, matches a sequence
    # of 0 or 1 matches of the atom
    (r'bar(s)?', r'bar', r''),
    (r'bar(s)?', r'bars', r's'),
    # + matches a sequence of 1 or more matches of the atom
    (r'(cin)+', r'cin', r'cin'),
    (r'((cin)+)', r'cincin', r'cincin cin'),
    # * matches a sequence of 0 or more matches of the atom
    (r'(0)*', r'1', r''),
    (r'(0)*', r'1000', r''),     # NOTE: this does not work as expected
                                 #       at the beginning of the pattern
    (r'1(0)*', r'1000', r'0'),
    (r'1(0)*1$', r'1001', r'0'),
    # \< matches at the beginning of a word
    (r'\\<(lo)', r'hello', r''),
    (r'\\<(lo)', r"she's so lovely", r'lo'),
    # \> matches at the end of a word
    (r'"\\>( fi)"', r'fidel', r''),
    (r'"\\>( fi)"', r'"hi fi"', r' fi'), # NOTE: extra space in result too
    # | separate branches, matches anything that matches one of the branches
    (r'(left)|(right)', r'left', r'left'),
    (r'(left)|(right)', r'right', r' right'), # NOTE: extra space as first group is empty
    # [] character class (list of characters enclosed in []), matches
    # any single character from the list. If the list begins with ^, it
    # matches any single character not from the rest of the list.
    # If two characters in the list are separated by -, this is shorthand
    # for the full range of characters between those two.
    # To include a literal ] in the list, make it the first character
    # (following a possible ^). To include a literal -, make it the first
    # or last character. Within brackets special characters ^.[$()|*+?
    # loose their special meaning.
    (r'"([0-9]+)"', r'1980s', r'1980'),
    (r'"([^0-9]+)"', r'1980s', r's'),
    # some real life cases
    (r'"^([0-9]+)\\.([0-9]+)(.*)$"', r'5.4.3beta', r'5 4 .3beta'),
    (r'^@(.*)', r'@my-rule', r'my-rule'),
    (r'^(!)?(.*)', r'"!bla bla"', r'! bla bla'),
    # portable EOL matching
    (r'"^([^\r\n]*)"', r'"hello\rworld"', r'hello'), # on Mac value
    (r'"^([^\r\n]*)"', r'"hello\nworld"', r'hello'), # on *nix value
    (r'"^([^\r\n]*)"', r'"hello\r\nworld"', r'hello'), # on Windows value
]

# Do not change code below !

testln = []
exptln = []
for n, c in enumerate(trials):
    testln.append('ECHO {} [ MATCH {} : {} ] ;'.format(n, *c))
    exptln.append('{} {}'.format(n, c[2]) if c[2] else str(n))
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

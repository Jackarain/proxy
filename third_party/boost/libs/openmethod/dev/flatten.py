#!/usr/bin/python3

import argparse
from pathlib import Path
import re

parser = argparse.ArgumentParser()
parser.add_argument("output", type=Path)
parser.add_argument("input", nargs="+", type=Path)
args = parser.parse_args()

prefix = args.input[0].absolute()

while prefix.name != "boost":
    assert prefix.parent != prefix
    prefix = prefix.parent

prefix = prefix.parent
skip = len(str(prefix)) + 1


def flatten(input, output, done):
    header = str(input)[skip:]
    if header in done:
        return
    done.add(header)
    with input.open() as ifh:
        for line in ifh:
            if input.name != "openmethod.hpp" and re.match(
                r"#include <(boost/openmethod/core\.hpp+)>", line
            ):
                continue

            if m := re.match(r"#include <(boost/openmethod/[^>]+)>", line):
                include = m[1]
                print(file=output)
                flatten(prefix / include, output, done)
                print(file=output)
                continue

            output.write(line)


with args.output.open("w") as ofh:
    done = set()
    for input in args.input:
        flatten(input.absolute(), ofh, done)

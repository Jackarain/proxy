#!/usr/bin/env python3
"""裁剪 Boost superproject，只保留本工程用到的库。

依赖闭包的计算规则与 BoostRoot.cmake 中的 __boost_scan_dependencies 保持一致：
扫描各库 CMakeLists.txt 中的 Boost::<lib> 引用（忽略注释），再按规则归一化库名。

用法:
  prune_boost.py --list <boost_dir> <lib> [lib ...]   只打印保留的库名
  prune_boost.py <boost_dir> <lib> [lib ...]          按闭包裁剪目录树
"""

import os
import re
import shutil
import sys

DEP_RE = re.compile(r"Boost::([A-Za-z0-9_/]+)")
MARKER_RE = re.compile(r"^ *# *Boost-(Include|Exclude):? *(.*)$")
TEST_RE = re.compile(r"(included_)?(unit_test_framework|prg_exec_monitor|test_exec_monitor)$")
NUMERIC_RE = re.compile(r"^numeric_(.+)$")
PREFIX_NAMES = ("asio", "dll", "fiber", "log", "regex", "stacktrace")

# 只服务于 b2/测试/文档/上游 CI，CMake 构建不需要。
DROP_SUBDIRS = (
    "test", "example", "bench", "fuzzing", "doc",
    ".drone", ".github", ".cirrus.yml", ".travis.yml", "appveyor.yml",
)
DROP_ROOT_DIRS = ("doc", "more", "status", ".circleci", ".github")
DROP_ROOT_FILES = (
    "INSTALL", "Jamroot", ".gitmodules", ".travis.yml", "appveyor.yml",
    "boost-build.jam", "boost.css", "boost.png", "boostcpp.jam",
    "bootstrap.bat", "bootstrap.sh", "index.htm", "index.html", "rst.css",
)


def normalize(dep):
    if dep in ("headers", "boost") or "linking" in dep:
        return None
    if TEST_RE.match(dep):
        return "test"
    if dep == "numpy":
        return "python"
    match = NUMERIC_RE.match(dep)
    if match:
        return "numeric/" + match.group(1)
    for prefix in PREFIX_NAMES:
        if dep.startswith(prefix + "_"):
            return prefix
    return dep


def scan_deps(libs_dir, lib):
    path = os.path.join(libs_dir, lib, "CMakeLists.txt")
    if not os.path.exists(path):
        return set()
    deps = set()
    excludes = set()
    with open(path, encoding="utf-8", errors="replace") as handle:
        for line in handle:
            marker = MARKER_RE.match(line)
            if marker:
                kind, line = marker.group(1), marker.group(2)
            else:
                kind = "Include"
            body = re.match(r"^([^#]*Boost::[A-Za-z0-9_]+[^#]*)(#.*)?$", line)
            if not body:
                continue
            for raw in DEP_RE.findall(body.group(1)):
                dep = normalize(raw)
                if dep and dep != lib:
                    (excludes if kind == "Exclude" else deps).add(dep)
    return deps - excludes


def collect(boost_dir, direct):
    libs_dir = os.path.join(boost_dir, "libs")
    keep = {"headers"}
    queue = list(direct)
    while queue:
        lib = queue.pop()
        if lib in keep:
            continue
        if not os.path.isdir(os.path.join(libs_dir, lib)):
            sys.stderr.write("boost: 未知库 '{}'，忽略\n".format(lib))
            continue
        keep.add(lib)
        queue.extend(scan_deps(libs_dir, lib) - keep)
    return keep


def remove(path):
    if os.path.islink(path) or os.path.isfile(path):
        os.remove(path)
    elif os.path.isdir(path):
        shutil.rmtree(path)


def prune(boost_dir, keep):
    libs_dir = os.path.join(boost_dir, "libs")
    for name in os.listdir(libs_dir):
        path = os.path.join(libs_dir, name)
        if not os.path.isdir(path) or name in keep:
            continue
        if name == "numeric":
            for sub in os.listdir(path):
                if "numeric/" + sub not in keep:
                    remove(os.path.join(path, sub))
        else:
            remove(path)

    for lib in keep:
        for sub in DROP_SUBDIRS:
            remove(os.path.join(libs_dir, lib, sub))

    for name in DROP_ROOT_DIRS:
        remove(os.path.join(boost_dir, name))
    for name in DROP_ROOT_FILES:
        remove(os.path.join(boost_dir, name))

    tools_dir = os.path.join(boost_dir, "tools")
    for name in os.listdir(tools_dir):
        if name != "cmake":
            remove(os.path.join(tools_dir, name))


def main():
    args = sys.argv[1:]
    listing = False
    if args and args[0] == "--list":
        listing = True
        args = args[1:]
    if len(args) < 2:
        sys.stderr.write(__doc__)
        return 2
    boost_dir, direct = args[0], args[1:]
    missing = [lib for lib in direct
               if not os.path.isdir(os.path.join(boost_dir, "libs", lib))]
    if missing:
        sys.stderr.write("boost: 未知库: {}\n".format(" ".join(missing)))
        return 1
    keep = collect(boost_dir, direct)
    if listing:
        print(" ".join(sorted(keep)))
        return 0
    prune(boost_dir, keep)
    print("boost: 保留 {} 个库: {}".format(len(keep), " ".join(sorted(keep))))
    return 0


if __name__ == "__main__":
    sys.exit(main())

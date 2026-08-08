#!/usr/bin/env python3
# Copyright 2026 Matt Borland
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt
#
# Updates the benchmark tables in ../pages/benchmarks.adoc from the .txt
# benchmark outputs in this directory.
#
# Output formats recognized (all share the same "<type>: N us" row layout):
#   * Boost.Decimal (test/benchmarks.cpp):     "comparisons<float        >: 90576 us"
#   * Intel libbid  (test/benchmark_libbid.c): "Comparisons    <Decimal32  >: 6072528 us"
#   * GCC _Decimal  (test/benchmark_libdfp.c): "Comparisons    <_Decimal32 >: 831820 us"
#
# For every table that has data, the Runtime (us) cell is updated for each row
# the data covers, then the Ratio to double cell is recomputed for every row in
# that table against the table's double runtime, so the ratio column stays
# internally consistent. Rows with no data (e.g. GCC _Decimal*), the ARM64 macOS
# section and the from_chars/to_chars tables are left as-is.
#
# Run: python3 update_benchmarks.py

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ADOC = os.path.normpath(os.path.join(HERE, "..", "pages", "benchmarks.adoc"))

# Operation tables handled (matches "===== Op =====" in data, "=== Op" in adoc).
OPS = ["Comparisons", "Addition", "Subtraction", "Multiplication", "Division"]

# Data type label (left of ":" in the .txt files) -> exact adoc "Type" cell text.
TYPE_TO_CELL = {
    "float":        "`float`",
    "double":       "`double`",
    "decimal32_t":  "`decimal32_t`",
    "decimal64_t":  "`decimal64_t`",
    "decimal128_t": "`decimal128_t`",
    "dec32_fast":   "`decimal_fast32_t`",
    "dec64_fast":   "`decimal_fast64_t`",
    "dec128_fast":  "`decimal_fast128_t`",
    "Decimal32":    "Intel `BID_UINT32`",
    "Decimal64":    "Intel `BID_UINT64`",
    "Decimal128":   "Intel `BID_UINT128`",
    "_Decimal32":   "GCC `_Decimal32`",
    "_Decimal64":   "GCC `_Decimal64`",
    "_Decimal128":  "GCC `_Decimal128`",
}

# Section anchors that are updated.
SECTION_ANCHOR = {
    "x64_linux":     "[#x64_linux_benchmarks]",
    "x32_linux":     "[#x32_linux_benchmarks]",
    "x64_windows":   "[#x64_windows_benchmarks]",
    "arm64_windows": "[#arm64_windows_benchmarks]",
    # The macos workflow job drives the ARM64 macOS tables (replacing the
    # earlier manual M4 Max numbers).
    "macos":         "[#m4_mac_benchmarks]",
}
# Anchors used only to bound sections (not updated: no data files for them).
OTHER_ANCHORS = ["[#m4_mac_benchmarks]"]

# Data file -> (section key, sub-table label or None for single-table sections).
# Files present in this folder but not listed here are reported and skipped.
FILE_MAP = {
    "benchmarks-linux-icpx-64-bit.txt":       ("x64_linux", "Intel Compiler"),
    "benchmarks-libbid-linux-icx-64-bit.txt": ("x64_linux", "Intel Compiler"),
    "benchmarks-linux-gcc-64-bit.txt":        ("x64_linux", "GCC"),
    "benchmarks-libbid-linux-gcc-64-bit.txt": ("x64_linux", "GCC"),
    "benchmarks-libdfp-linux-gcc-64-bit.txt": ("x64_linux", "GCC"),
    "benchmarks-linux-gcc-32-bit.txt":        ("x32_linux", None),
    "benchmarks-libbid-linux-gcc-32-bit.txt": ("x32_linux", None),
    "benchmarks-libdfp-linux-gcc-32-bit.txt": ("x32_linux", None),
    "benchmarks-windows-x64.txt":             ("x64_windows", None),
    "benchmarks-libbid-windows-x64.txt":      ("x64_windows", None),
    "benchmarks-windows-arm64.txt":           ("arm64_windows", None),
    "benchmarks-macos-arm64.txt":             ("macos", None),
}

OP_HEADER_RE = re.compile(r"^=====\s*(.+?)\s*=====$")
DATA_ROW_RE = re.compile(r"<\s*([A-Za-z0-9_]+)\s*>\s*:\s*(\d+)\s*us")
ADOC_OP_RE = re.compile(r"^===\s+(\w+)\s*$")


def parse_data_file(path):
    # Return {op -> {cell_text -> runtime_us}} for one .txt output file.
    result = {}
    op = None
    with open(path) as fh:
        for raw in fh:
            line = raw.rstrip("\n")
            m = OP_HEADER_RE.match(line.strip())
            if m:
                op = m.group(1)
                continue
            if op not in OPS:
                continue
            m = DATA_ROW_RE.search(line)
            if not m:
                continue
            cell = TYPE_TO_CELL.get(m.group(1))
            if cell is not None:
                result.setdefault(op, {})[cell] = int(m.group(2))
    return result


def find_line(lines, needle, start=0):
    for i in range(start, len(lines)):
        if lines[i].strip() == needle:
            return i
    return -1


def section_spans(lines):
    # Return {section_key -> (start, end)} for the updated sections.
    positions = []
    for anchor in list(SECTION_ANCHOR.values()) + OTHER_ANCHORS:
        i = find_line(lines, anchor)
        if i >= 0:
            positions.append(i)
    positions.sort()

    def end_of(start):
        for p in positions:
            if p > start:
                return p
        return len(lines)

    spans = {}
    for key, anchor in SECTION_ANCHOR.items():
        i = find_line(lines, anchor)
        if i < 0:
            print("  WARNING: section anchor not found: " + anchor)
            continue
        spans[key] = (i, end_of(i))
    return spans


def op_spans(lines, start, end):
    # Return list of (op, op_start, op_end) for the basic-operation tables.
    headers = []
    for i in range(start, end):
        if lines[i].startswith("=="):
            m = ADOC_OP_RE.match(lines[i])
            headers.append((i, m.group(1) if m else None))
    out = []
    for j, (idx, op) in enumerate(headers):
        if op in OPS:
            nxt = headers[j + 1][0] if j + 1 < len(headers) else end
            out.append((op, idx, nxt))
    return out


def table_rows(lines, open_idx, close_idx):
    # Parse a |===...|=== table into [(cell, runtime_idx, ratio_idx, existing_us)].
    body = [i for i in range(open_idx + 1, close_idx) if lines[i].strip()]
    if not body:
        return None
    body = body[1:]  # drop the "| Type | Runtime (us) | Ratio to double" header
    if len(body) % 3 != 0:
        return None
    rows = []
    for k in range(0, len(body), 3):
        li_cell, li_rt, li_ratio = body[k], body[k + 1], body[k + 2]
        if not all(lines[x].lstrip().startswith("|") for x in (li_cell, li_rt, li_ratio)):
            return None
        cell = lines[li_cell].strip()[1:].strip()
        rt_text = lines[li_rt].strip()[1:].strip().replace(",", "")
        existing = int(rt_text) if rt_text.isdigit() else None
        rows.append((cell, li_rt, li_ratio, existing))
    return rows


def update_table(lines, open_idx, close_idx, data, label_for_report, report):
    rows = table_rows(lines, open_idx, close_idx)
    if rows is None:
        report["warnings"].append("could not parse table at line %d (%s)"
                                   % (open_idx + 1, label_for_report))
        return

    final = {}
    for cell, _, _, existing in rows:
        final[cell] = data.get(cell, existing)

    dbl = final.get("`double`")
    if not dbl:
        report["warnings"].append("no double runtime for table %s" % label_for_report)
        return

    for cell, li_rt, li_ratio, _ in rows:
        rt = final[cell]
        if rt is None:
            continue
        lines[li_rt] = "| {:,}".format(rt)
        lines[li_ratio] = "| {:.3f}".format(rt / dbl)
        if cell in data:
            report["runtime_rows"] += 1
        else:
            report["ratio_only"].append("%s / %s" % (label_for_report, cell))


def process_op(lines, op, start, end, section_key, table_data, report):
    label = None
    i = start
    while i < end:
        s = lines[i].strip()
        if s == "Intel Compiler:":
            label = "Intel Compiler"
        elif s == "GCC:":
            label = "GCC"
        elif s == "|===":
            close = find_line(lines, "|===", i + 1)
            if close < 0:
                break
            data = table_data.get((section_key, label), {}).get(op, {})
            tag = "%s/%s%s" % (section_key, op, ("/" + label) if label else "")
            if data:
                update_table(lines, i, close, data, tag, report)
                report["tables"] += 1
            i = close
        i += 1


def main():
    if not os.path.isfile(ADOC):
        sys.exit("ERROR: cannot find benchmarks.adoc at " + ADOC)

    # Build per-table data and report files that have no mapping.
    table_data = {}
    present = sorted(f for f in os.listdir(HERE) if f.endswith(".txt"))
    used = []
    for fname in present:
        if fname not in FILE_MAP:
            continue
        used.append(fname)
        section, label = FILE_MAP[fname]
        parsed = parse_data_file(os.path.join(HERE, fname))
        dst = table_data.setdefault((section, label), {})
        for op, rows in parsed.items():
            dst.setdefault(op, {}).update(rows)

    skipped = [f for f in present if f not in FILE_MAP]
    missing = [f for f in FILE_MAP if not os.path.isfile(os.path.join(HERE, f))]

    text = open(ADOC).read()
    lines = text.split("\n")

    report = {"tables": 0, "runtime_rows": 0, "ratio_only": [], "warnings": []}
    for section_key, (start, end) in section_spans(lines).items():
        for op, ostart, oend in op_spans(lines, start, end):
            process_op(lines, op, ostart, oend, section_key, table_data, report)

    new_text = "\n".join(lines)
    changed = new_text != text
    if changed:
        with open(ADOC, "w") as fh:
            fh.write(new_text)

    print("Data files used (%d):" % len(used))
    for f in used:
        print("  " + f)
    if skipped:
        print("Skipped (no doc section mapped):")
        for f in skipped:
            print("  " + f)
    if missing:
        print("Expected but missing:")
        for f in missing:
            print("  " + f)
    print("")
    print("Tables updated:        %d" % report["tables"])
    print("Runtime rows from data: %d" % report["runtime_rows"])
    if report["ratio_only"]:
        print("Ratio-only rows (no runtime data, recomputed vs new double):")
        for r in report["ratio_only"]:
            print("  " + r)
    if report["warnings"]:
        print("Warnings:")
        for w in report["warnings"]:
            print("  " + w)
    print("")
    print("benchmarks.adoc " + ("updated." if changed else "already up to date."))


if __name__ == "__main__":
    main()

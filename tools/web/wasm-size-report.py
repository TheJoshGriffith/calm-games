#!/usr/bin/env python3
"""Explains what a WebAssembly binary is made of, as Markdown for a CI job summary.

    python3 tools/web/wasm-size-report.py App.wasm [--map link.map] [--top 25]

Without a map it lists the binary's sections (code, data, names, ...). With a
wasm-ld link map (-Wl,--Map=link.map) it also adds up code and data by the
library or object file they came from: Qt modules, Qt plugins, resources
(qrc_*) and our own code. The map describes the linker's output, before
wasm-opt shrinks the code, so its numbers are shares rather than final sizes.
"""

import argparse
import collections
import re
import sys
from pathlib import Path

SECTION_NAMES = {
    0: "custom", 1: "type", 2: "import", 3: "function", 4: "table", 5: "memory",
    6: "global", 7: "export", 8: "start", 9: "elem", 10: "code", 11: "data",
    12: "datacount", 13: "tag",
}


def leb128(data, pos):
    result = shift = 0
    while True:
        byte = data[pos]
        pos += 1
        result |= (byte & 0x7F) << shift
        shift += 7
        if not byte & 0x80:
            return result, pos


def sections(path):
    """(name, size) for every section of a .wasm file."""
    data = Path(path).read_bytes()
    if data[:4] != b"\0asm":
        sys.exit(f"{path} is not a WebAssembly binary")
    pos, out = 8, []
    while pos < len(data):
        sid = data[pos]
        size, body = leb128(data, pos + 1)
        name = SECTION_NAMES.get(sid, f"id {sid}")
        if sid == 0:
            length, p = leb128(data, body)
            name = "custom: " + data[p:p + length].decode("utf-8", "replace")
        out.append((name, body + size - pos))
        pos = body + size
    return out


def group_of(chunk):
    """Where a linker input came from: a library, a Qt plugin, resources or an object."""
    path = chunk.rsplit(":(", 1)[0]
    member = None
    m = re.match(r"(.*)\(([^()]*)\)$", path)
    if m:
        path, member = m.group(1), m.group(2)
    name = path.rsplit("/", 1)[-1]
    if "/.rcc/" in path or re.match(r"qrc_", member or name):
        res = re.sub(r"\.cpp\.o(bj)?$", "", member or name)
        return "resources: " + re.sub(r"^qrc_", "", res)
    if name.endswith(".a"):
        lib = re.sub(r"^lib|\.a$", "", name)
        if "/plugins/" in path:
            return "plugin: " + path.split("/plugins/", 1)[1].rsplit("/", 1)[0] + "/" + lib
        if "/qml/" in path:
            return "qml plugin: " + lib
        return lib
    # Object files from CMake object libraries: name them after their target.
    m = re.search(r"/CMakeFiles/([^/]+)\.dir/", path)
    if m:
        return "objects: " + m.group(1)
    m = re.search(r"/objects-[^/]+/([^/]+)/", path)
    if m:
        return "objects: " + m.group(1)
    return name


def read_map(path):
    """Sizes by (output section, group) and by input chunk from a wasm-ld map."""
    by_group = collections.Counter()
    by_chunk = collections.Counter()
    section = segment = None
    line_re = re.compile(r"^\s*(\S+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+) (.*)$")
    for line in Path(path).read_text(errors="replace").splitlines():
        m = line_re.match(line)
        if not m or m.group(1) == "Addr":
            continue
        size = int(m.group(3), 16)
        rest = m.group(4)
        indent = len(rest) - len(rest.lstrip(" "))
        if indent == 0:
            name = rest.strip()
            if name.isupper() or name.startswith("CUSTOM"):
                section, segment = name, None
            else:
                segment = name      # a data segment such as .rodata or .bss
            continue
        if indent != 8:             # 16: a symbol inside the chunk above
            continue
        chunk = rest.strip()
        if section == "DATA" and segment == ".bss":
            continue                # zero-initialised: not in the binary
        if section in ("CODE", "DATA"):
            group = group_of(chunk)
            by_group[(section, group)] += size
            by_chunk[(section, chunk)] += size
    return by_group, by_chunk


def mb(n):
    return f"{n / 1e6:.2f} MB"


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("wasm")
    parser.add_argument("--map")
    parser.add_argument("--top", type=int, default=25)
    parser.add_argument("--title", default=None)
    args = parser.parse_args()

    secs = sections(args.wasm)
    total = sum(s for _, s in secs)
    print(f"#### {args.title or Path(args.wasm).name}: {total} bytes\n")
    print("| Section | Size | Share |\n| --- | ---: | ---: |")
    merged = collections.Counter()
    for name, size in secs:
        merged[name] += size
    for name, size in merged.most_common():
        if size >= 1000:
            print(f"| {name} | {mb(size)} | {100 * size / total:.1f}% |")
    print()

    if not args.map:
        return
    by_group, by_chunk = read_map(args.map)
    for section in ("CODE", "DATA"):
        rows = [(g, s) for (sec, g), s in by_group.items() if sec == section]
        rows.sort(key=lambda r: -r[1])
        sec_total = sum(s for _, s in rows)
        print(f"<details><summary>{section.lower()} by library ({mb(sec_total)} before wasm-opt)</summary>\n")
        print("| From | Size | Share |\n| --- | ---: | ---: |")
        for g, s in rows[:args.top]:
            print(f"| {g} | {mb(s)} | {100 * s / max(sec_total, 1):.1f}% |")
        rest = sum(s for _, s in rows[args.top:])
        if rest:
            print(f"| {len(rows) - args.top} more | {mb(rest)} | {100 * rest / max(sec_total, 1):.1f}% |")
        print("\n</details>\n")
    rows = [(c, s) for (sec, c), s in by_chunk.items() if sec == "DATA"]
    rows.sort(key=lambda r: -r[1])
    print("<details><summary>largest data inputs</summary>\n")
    print("| Input | Size |\n| --- | ---: |")
    for c, s in rows[:args.top]:
        short = re.sub(r"^.*/", "", c.rsplit(":(", 1)[0]) + (":(" + c.rsplit(":(", 1)[1] if ":(" in c else "")
        print(f"| `{short[:120]}` | {mb(s)} |")
    print("\n</details>\n")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Compile Sly decompilation C++ directly for a native 32-bit host ABI."""

import argparse
import re
import shlex
import subprocess
import sys
from pathlib import Path


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path, help="read-only sly1-decomp checkout")
    parser.add_argument("--out", required=True, type=Path, help="staged source and object output directory")
    parser.add_argument("--port", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--cxx", default="c++")
    args = parser.parse_args(argv)

    source = args.source.expanduser().resolve()
    out = args.out.expanduser().resolve()
    port = args.port.resolve()
    include = source / "include"
    sources = sorted((source / "src").rglob("*.cpp"))
    if not sources:
        parser.error("no C++ source units found under " + str(source / "src"))
    compiler = shlex.split(args.cxx)
    failures = []
    warnings = 0
    staged_root = out / "staged"
    objects_root = out / "objects"
    flags = [
        "-m32", "-msse2", "-std=gnu++20", "-fno-exceptions", "-fno-rtti",
        "-fno-pie", "-fno-pic", "-ffunction-sections", "-fdata-sections",
        "-fno-asynchronous-unwind-tables", "-fno-unwind-tables", "-fpermissive",
        "-include", str(port / "sly1" / "host_abi.h"),
        "-I", str(include),
    ]
    stamp = out / "compile.command.txt"
    stamp.parent.mkdir(parents=True, exist_ok=True)
    command_description = "\n".join(compiler + flags) + "\n"
    if not stamp.exists() or stamp.read_text(encoding="utf-8") != command_description:
        stamp.write_text(command_description, encoding="utf-8")

    for src in sources:
        rel = src.relative_to(source / "src")
        staged = staged_root / rel
        obj = objects_root / rel.with_suffix(".o")
        staged.parent.mkdir(parents=True, exist_ok=True)
        obj.parent.mkdir(parents=True, exist_ok=True)
        original = src.read_text(encoding="utf-8")
        # The PS2 compiler treats unsigned long literals as 64-bit; i386 does not.
        adapted = re.sub(r"(?i)(0x[0-9a-f]+|[0-9]+)UL\b", r"\1ULL", original)
        if not staged.exists() or staged.read_text(encoding="utf-8") != adapted:
            staged.write_text(adapted, encoding="utf-8")
        deps = [src, staged, port / "sly1" / "host_abi.h", stamp]
        if obj.exists() and obj.stat().st_mtime >= max(p.stat().st_mtime for p in deps):
            continue
        cmd = compiler + flags + ["-I", str(src.parent), "-c", str(staged), "-o", str(obj)]
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if proc.returncode:
            failures.append((rel.as_posix(), proc.stderr))
        else:
            warnings += proc.stderr.count("warning:")
            if proc.stderr:
                print(proc.stderr, file=sys.stderr, end="")

    print(f"Sly host C++ units: {len(sources) - len(failures)}/{len(sources)} compiled; {warnings} compiler warnings")
    if failures:
        for name, error in failures:
            print(f"\n{name}:\n{error}", file=sys.stderr)
        return 1
    print(f"staged sources: {staged_root}")
    print(f"object files: {objects_root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

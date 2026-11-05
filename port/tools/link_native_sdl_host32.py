#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Link the direct 32-bit Sly build to SDL3 and OpenRAC's GS renderer."""

import argparse
import re
import shlex
import subprocess
import sys
from pathlib import Path

FUNCTION = re.compile(r"(?:_Z13func_|func_)([0-9A-Fa-f]{8})")


def run(command):
    result = subprocess.run(command, text=True, capture_output=True)
    if result.returncode:
        sys.stderr.write(result.stderr)
        return False
    if result.stderr:
        sys.stderr.write(result.stderr)
    return True


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--object", required=True, type=Path)
    parser.add_argument("--runtime", required=True, type=Path,
                        help="32-bit libsly1_port_runtime.a built with direct retail memory")
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--port", required=True, type=Path)
    parser.add_argument("--cxx", default="c++")
    parser.add_argument("--cxxflags", default="")
    parser.add_argument("--sysroot", type=Path)
    parser.add_argument("--runtime-includes", type=Path, action="append", default=[])
    parser.add_argument("--sdl-libdir", type=Path,
                        help="directory containing the 32-bit libSDL3.so")
    args = parser.parse_args(argv)
    args.out.mkdir(parents=True, exist_ok=True)

    nm = subprocess.run(["nm", "--undefined-only", str(args.object)],
                        text=True, capture_output=True, check=True)
    missing = {}
    for line in nm.stdout.splitlines():
        fields = line.split()
        if len(fields) >= 2:
            match = FUNCTION.search(fields[-1])
            if match:
                missing[fields[-1]] = int(match.group(1), 16)
    asm = [".section .text.missing,\"ax\",@progbits"]
    for symbol, address in sorted(missing.items()):
        asm.extend([f".globl {symbol}", f".type {symbol},@function", f"{symbol}:",
                    f"    pushl $0x{address:08x}", "    call sly1_missing_function", "    ud2"])
    missing_asm = args.out / "missing_functions.S"
    missing_asm.write_text("\n".join(asm) + "\n", encoding="utf-8")

    compiler = shlex.split(args.cxx)
    common = ["-m32", "-msse2", "-std=gnu++20", "-fno-pie", "-fno-pic",
              "-fno-exceptions", "-fno-rtti", "-DOPENRAC_SLY1_WITH_SDL"]
    if args.sysroot:
        common.append(f"--sysroot={args.sysroot.resolve()}")
    common += shlex.split(args.cxxflags)
    common += ["-I", str(args.port), "-I", str(args.port / "runtime" / "include")]
    for include in args.runtime_includes:
        common += ["-I", str(include)]

    objects = []
    for name, source in (("main.o", args.source / "main.cpp"),
                         ("abi_compat.o", args.source / "abi_compat.cpp"),
                         ("syscalls.o", args.source / "start.S"),
                         ("clone.o", args.source / "clone.S"),
                         ("missing_functions.o", missing_asm)):
        obj = args.out / name
        command = compiler + common + ["-c", str(source), "-o", str(obj)]
        if not run(command):
            return 1
        objects.append(obj)

    executable = args.out / "sly1_native_sdl32"
    command = compiler + common + ["-no-pie", "-Wl,--build-id=none", "-o", str(executable),
                                   *map(str, objects), str(args.object), str(args.runtime),
                                   "-lSDL3", "-pthread", "-ldl"]
    if args.sdl_libdir:
        command[command.index("-lSDL3"):command.index("-lSDL3")] = ["-L", str(args.sdl_libdir)]
    if args.sysroot:
        command.insert(command.index("-no-pie"), f"--sysroot={args.sysroot.resolve()}")
    if not run(command):
        return 1
    print(f"linked {executable}; SDL3 renderer and {len(missing)} missing-function traps included")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

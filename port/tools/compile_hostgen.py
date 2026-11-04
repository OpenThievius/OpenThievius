# SPDX-License-Identifier: AGPL-3.0-or-later
"""Compile hostgen's local output into ignored object files."""

import argparse
import os
import shlex
import subprocess
import sys
from pathlib import Path


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--generated", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--runtime-includes", type=Path, required=True)
    args = parser.parse_args(argv)
    compiler = shlex.split(os.environ.get("CC", "cc"))
    units = [line for line in (args.generated / "units.txt").read_text().splitlines() if line]
    units += ["stubs.c", "functions.c", "hooks.c", "libraries.c", "game_info.c"]
    args.out.mkdir(parents=True, exist_ok=True)
    for rel in units:
        source = args.generated / rel
        target = args.out / (rel.replace("/", "_") + ".o")
        command = compiler + [
            "-std=gnu11", "-Dbool=_Bool", "-Wno-error=implicit-function-declaration",
            "-I", str(args.generated), "-I", str(args.runtime_includes),
            "-c", str(source), "-o", str(target),
        ]
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode:
            print("error: compiling %s" % rel, file=sys.stderr)
            sys.stderr.write(result.stderr or result.stdout)
            return result.returncode
    print("compiled %d generated C translation units" % len(units))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

# SPDX-License-Identifier: AGPL-3.0-or-later
"""Link generated host code to catch missing definitions without running the game."""

import argparse
import subprocess
from pathlib import Path


SUPPORT_UNITS = ("stubs.c", "functions.c", "hooks.c", "libraries.c", "game_info.c")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--generated", type=Path, required=True)
    parser.add_argument("--objects", type=Path, required=True)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--runtime-includes", type=Path, required=True)
    parser.add_argument("--stub", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--cxx", required=True)
    parser.add_argument("--link-library", action="append", default=[])
    args = parser.parse_args(argv)

    units = [line for line in (args.generated / "units.txt").read_text().splitlines() if line]
    units += SUPPORT_UNITS
    objects = [args.objects / (rel.replace("/", "_") + ".o") for rel in units]
    missing = [path for path in objects if not path.is_file()]
    if missing:
        parser.error("generated object is missing: %s" % missing[0])

    args.out.parent.mkdir(parents=True, exist_ok=True)
    command = [args.cxx, "-std=c++20", "-pthread", "-I", str(args.runtime_includes),
               "-I", str(args.generated), str(args.stub), *map(str, objects), str(args.runtime),
               *["-l" + library for library in args.link_library if library],
               "-o", str(args.out)]
    subprocess.run(command, check=True)
    print("linked %d generated C units into %s (not run)" % (len(units), args.out))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Combine native decompilation objects and resolve symbols by retail address."""

import argparse
import json
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

FUNCTION_ADDRESS = re.compile(r"func_([0-9A-Fa-f]{8})")
DATA_ADDRESS = re.compile(r"D_([0-9A-Fa-f]{8})")


def nm_symbols(nm, mode, path):
    result = subprocess.run([nm, mode, str(path)], check=True, text=True, capture_output=True)
    symbols = set()
    for line in result.stdout.splitlines():
        fields = line.split()
        if fields:
            symbols.add(fields[-1])
    return symbols


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--objects", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--nm", default="nm")
    parser.add_argument("--ld", default="ld")
    args = parser.parse_args(argv)

    objects = sorted(args.objects.resolve().rglob("*.o"))
    if not objects:
        parser.error("no host32 object files found")
    defined_by_address = defaultdict(set)
    undefined = set()
    for obj in objects:
        for symbol in nm_symbols(args.nm, "--defined-only", obj):
            match = FUNCTION_ADDRESS.search(symbol)
            if match:
                defined_by_address[int(match.group(1), 16)].add(symbol)
        undefined.update(nm_symbols(args.nm, "--undefined-only", obj))

    aliases = []
    function_aliases = 0
    data_aliases = 0
    ambiguous = []
    for symbol in sorted(undefined):
        match = FUNCTION_ADDRESS.search(symbol)
        if match:
            address = int(match.group(1), 16)
            candidates = defined_by_address.get(address, set())
            if len(candidates) == 1:
                target = next(iter(candidates))
                if symbol != target:
                    aliases.append(f"PROVIDE({symbol} = {target});")
                    function_aliases += 1
            elif len(candidates) > 1:
                ambiguous.append({"address": f"0x{address:08x}", "references": symbol,
                                  "definitions": sorted(candidates)})
            continue
        match = DATA_ADDRESS.fullmatch(symbol)
        if match:
            aliases.append(f"PROVIDE({symbol} = 0x{int(match.group(1), 16):08x});")
            data_aliases += 1

    args.out.mkdir(parents=True, exist_ok=True)
    script = args.out / "retail-address-aliases.ld"
    script.write_text("\n".join(aliases) + "\n", encoding="utf-8")
    combined = args.out / "decomp-native32.o"
    cmd = [args.ld, "-m", "elf_i386", "-r", "-T", str(script), "-o", str(combined)]
    cmd.extend(map(str, objects))
    result = subprocess.run(cmd, text=True, capture_output=True)
    if result.returncode:
        print(result.stderr, file=sys.stderr)
        return result.returncode

    remaining = nm_symbols(args.nm, "--undefined-only", combined)
    missing_addresses = sorted({int(m.group(1), 16) for name in remaining
                                if (m := FUNCTION_ADDRESS.search(name))})
    report = {
        "input_objects": len(objects),
        "function_reference_aliases": function_aliases,
        "retail_data_address_aliases": data_aliases,
        "ambiguous_function_addresses": ambiguous,
        "unresolved_symbols": len(remaining),
        "unresolved_function_addresses": [f"0x{x:08x}" for x in missing_addresses],
        "combined_object": str(combined),
    }
    (args.out / "link-frontier.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"combined {len(objects)} i386 objects")
    print(f"resolved {function_aliases} function references and {data_aliases} retail data symbols")
    print(f"unresolved function addresses: {len(missing_addresses)} ({len(remaining)} symbol names)")
    print(f"combined object: {combined}")
    print(f"frontier report: {args.out / 'link-frontier.json'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

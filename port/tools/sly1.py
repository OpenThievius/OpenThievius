# SPDX-License-Identifier: AGPL-3.0-or-later
"""Inspect the Sly 1 decompilation inputs needed by the native port.

    python3 port/tools/sly1.py status [--version us|eu] [--source DIR]

This deliberately reads only committed source and metadata. It does not read
baserom/, asm/, assets/ or build/ from the decompilation.
"""

import argparse
import json
import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_SOURCE = ROOT.parent / "sly1-decomp"
FUNCTION = re.compile(r"\b(func_[0-9A-Fa-f]{8})\b")


def source_root(explicit):
    value = explicit or os.environ.get("OPENTHIEVIUS_SOURCE_SLY1")
    return Path(value).expanduser().resolve() if value else DEFAULT_SOURCE.resolve()


def read_functions(source, version):
    path = source / "config" / version / "functions.tsv"
    if not path.is_file():
        raise FileNotFoundError("missing %s" % path)
    functions = []
    for line in path.read_text(encoding="utf-8").splitlines()[1:]:
        if not line:
            continue
        columns = line.split("\t")
        functions.append((columns[0], int(columns[1], 16), int(columns[2]), columns[3]))
    return functions


def status(source, version):
    functions = read_functions(source, version)
    source_files = sorted((source / "src").rglob("*.cpp"))
    source_text = "\n".join(path.read_text(encoding="utf-8", errors="replace") for path in source_files)
    defined = set(FUNCTION.findall(source_text))
    linked = {name for name, _, _, _ in functions if name in defined}
    code_functions = [row for row in functions if row[3] in ("code", "handwritten")]
    matched = [row for row in code_functions if row[0] in linked]
    report_path = source / "progress" / version / "report.json"
    report = json.loads(report_path.read_text(encoding="utf-8")) if report_path.is_file() else None
    total_bytes = sum(row[2] for row in code_functions)
    matched_bytes = sum(row[2] for row in matched)
    if report:
        measures = report.get("measures", {})
        total_functions = int(measures.get("total_functions", len(code_functions)))
        matched_functions = int(measures.get("matched_functions", len(matched)))
        total_bytes = int(measures.get("total_code", total_bytes))
        matched_bytes = int(measures.get("matched_code", matched_bytes))
    else:
        total_functions = len(code_functions)
        matched_functions = len(matched)
    return {
        "version": version,
        "source_files": len(source_files),
        "functions_total": total_functions,
        "functions_with_c": matched_functions,
        "functions_without_c": total_functions - matched_functions,
        "code_bytes_total": total_bytes,
        "code_bytes_with_c": matched_bytes,
        "code_percent_with_c": round(100 * matched_bytes / total_bytes, 2) if total_bytes else 0,
        "function_percent_with_c": round(100 * matched_functions / total_functions, 2) if total_functions else 0,
        "progress_report_available": report is not None,
        "data_sections": [row for row in functions if row[3] not in ("code", "handwritten")],
        "portability_note": "C coverage is not host readiness; pointer layout, assembly fallbacks, and library replacements remain unimplemented.",
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("status", help="report decompilation inputs and current code coverage")
    p.add_argument("--version", choices=("us", "eu"), default="us")
    p.add_argument("--source", help="sly1-decomp checkout (or OPENTHIEVIUS_SOURCE_SLY1)")
    p.add_argument("--json", action="store_true", help="print machine-readable JSON")
    args = parser.parse_args(argv)
    source = source_root(args.source)
    try:
        result = status(source, args.version)
    except (FileNotFoundError, ValueError, json.JSONDecodeError) as error:
        print("error: %s" % error, file=sys.stderr)
        return 1
    if args.json:
        print(json.dumps(result, indent=2, sort_keys=True))
    else:
        print("source: %s" % source)
        print("version: %s" % result["version"])
        print("C++ source files: %d" % result["source_files"])
        print("matched C: %d/%d functions (%.2f%% code bytes)" % (
            result["functions_with_c"], result["functions_total"], result["code_percent_with_c"]))
        print("functions without C: %d" % result["functions_without_c"])
        print("committed progress report: %s" % ("available" if result["progress_report_available"] else "missing"))
        print("port readiness: %s" % result["portability_note"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

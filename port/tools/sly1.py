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
import shutil
import subprocess
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


def prepare_c_view(source, version):
    """Stage Sly's C-like C++ units as C for the current hostgen frontend."""
    staged = ROOT / "build" / "port" / "source" / version
    if staged.exists():
        shutil.rmtree(staged)
    (staged / "src").mkdir(parents=True)
    (staged / "include").mkdir()
    (staged / "config" / version).mkdir(parents=True)
    for path in (f"config/{version}/symbol_addrs.txt",):
        shutil.copy2(source / path, staged / path)
    # Newer decomp snapshots use shared class and helper headers in whole-file
    # units. Stage those headers too; copying only common.h made hostgen reject
    # otherwise readable C units and silently replace their functions with
    # no-C stubs. The upstream checkout remains read-only.
    source_includes = source / "include"
    header_texts = {}
    for header in sorted(source_includes.rglob("*")):
        if not header.is_file() or header.suffix not in (".h", ".hpp", ".inc"):
            continue
        header_text = header.read_text(encoding="utf-8")
        # The C-like decomp headers use C++ inheritance and one-line accessors.
        # Keep the upstream checkout untouched; convert those constructs only
        # in the disposable C staging tree below.
        header_text = re.sub(
            r"^\s*[^\n{};]+\([^\n{};]*\)\s*(?:const\s*)?\{[^\n{}]*\}\s*$\n?",
            "",
            header_text,
            flags=re.MULTILINE,
        )
        header_texts[header] = header_text

    def struct_bodies(text):
        """Find complete named struct bodies, including nested aggregates."""
        result = {}
        start_re = re.compile(
            r"\bstruct\s+(?:__attribute__\s*\(\(.*?\)\)\s*)?"
            r"([A-Za-z_]\w*)(?:\s*:\s*[A-Za-z_]\w*)?\s*\{"
        )
        for match in start_re.finditer(text):
            depth = 1
            pos = match.end()
            body_start = pos
            while pos < len(text) and depth:
                if text[pos] == "{":
                    depth += 1
                elif text[pos] == "}":
                    depth -= 1
                pos += 1
            if depth == 0:
                result[match.group(1)] = text[body_start:pos - 1]
        return result

    inheritance = re.compile(
        r"(\bstruct\s+[A-Za-z_]\w*)\s*:\s*([A-Za-z_]\w*)\s*\{"
    )
    derived = {}
    for text in header_texts.values():
        for match in inheritance.finditer(text):
            tag = re.search(r"struct\s+([A-Za-z_]\w*)$", match.group(1)).group(1)
            derived[tag] = match.group(2)
    ready = set(derived.values()) - set(derived)
    for _ in range(8):
        base_bodies = struct_bodies("\n".join(header_texts.values()))
        changed = False
        flattened = set()
        for header, header_text in list(header_texts.items()):
            def inline_base(match):
                nonlocal changed
                base = match.group(2)
                body = base_bodies.get(base)
                if body is None or base not in ready:
                    return match.group(0)
                tag = re.search(r"struct\s+([A-Za-z_]\w*)$", match.group(1)).group(1)
                flattened.add(tag)
                changed = True
                return match.group(1) + " {\n    struct {" + body + "\n    };"
            header_texts[header] = inheritance.sub(inline_base, header_text)
        ready.update(flattened)
        if not changed:
            break

    for header, header_text in header_texts.items():
        target = staged / "include" / header.relative_to(source_includes)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(header_text, encoding="utf-8")
    shared_tags = set()
    for header in sorted((staged / "include").rglob("*")):
        if not header.is_file() or header.suffix not in (".h", ".hpp", ".inc"):
            continue
        shared_tags.update(re.findall(
            r"\b(struct|union|enum)\s+(?:__attribute__\s*\(\(.*?\)\)\s*)?"
            r"([A-Za-z_]\w*)\s*(?=;|\{)",
            header.read_text(encoding="utf-8")))
    function_table = (source / "config" / version / "functions.tsv").read_text(encoding="utf-8").splitlines()
    catalogue_rows = []
    for line in function_table[1:]:
        if not line:
            continue
        columns = line.split("\t")
        columns[1] = "0x" + columns[1]
        catalogue_rows.append("\t".join(columns[:3]))
    (staged / "config" / version / "functions.tsv").write_text("\n".join(catalogue_rows) + "\n", encoding="utf-8")
    for directory in ("main", "lib"):
        out_dir = staged / "src" / directory
        out_dir.mkdir()
        for cpp in sorted((source / "src" / directory).glob("*.cpp")):
            text = cpp.read_text(encoding="utf-8")
            # C++ permits a bare forward-declared tag in later casts, while C
            # requires its typedef name. Collect declarations as well as
            # definitions so those casts remain valid in the staged C view.
            tags = sorted(set(re.findall(
                r"\b(struct|union|enum)\s+(?:__attribute__\s*\(\(.*?\)\)\s*)?"
                r"([A-Za-z_]\w*)\b",
                text,
            )))
            all_tags = sorted(set(tags) | shared_tags)
            prelude = "#define bool _Bool\n" + "".join(
                "typedef {0} {1} {1};\n".format(kind, tag)
                for kind, tag in all_tags)
            if cpp.stem == "00134000":
                # This unit's local C++ helpers wrap the retail stream logic.
                # Lower static methods into file-local C functions so hostgen
                # can read the matched code without changing its behavior.
                while True:
                    method = re.search(
                        r"struct\s+([A-Za-z_]\w*)\s*\{\s*"
                        r"static\s+([A-Za-z_]\w*(?:\s*\*)?)\s+"
                        r"([A-Za-z_]\w*)\s*\(([^{}]*)\)\s*\{",
                        text,
                    )
                    if method is None:
                        break
                    depth = 1
                    pos = method.end()
                    while pos < len(text) and depth:
                        if text[pos] == "{":
                            depth += 1
                        elif text[pos] == "}":
                            depth -= 1
                        pos += 1
                    if depth:
                        break
                    body_end = pos
                    tail = re.match(r"\s*};", text[pos:])
                    if tail is not None:
                        pos += tail.end()
                    method_name = f"{method.group(1)}_{method.group(3)}"
                    function = (
                        f"static {method.group(2).strip()} {method_name}"
                        f"({method.group(4)}) {{{text[method.end():body_end - 1]}}}"
                    )
                    text = text[:method.start()] + function + text[pos:]
                    text = text.replace(
                        f"{method.group(1)}::{method.group(3)}(",
                        f"{method_name}(",
                    )

                # This setter is unused here; retain the record field layout.
                state = re.search(r"struct\s+State_001365A8\s*\{", text)
                if state is not None:
                    close = text.find("};", state.end())
                    if close != -1:
                        body = text[state.end():close]
                        body = re.sub(
                            r"\s*void\s+set\s*\([^{}]*\)\s*\{.*?\}",
                            "",
                            body,
                            flags=re.DOTALL,
                        )
                        text = text[:state.end()] + body + text[close:]
            # The readable class headers expose these as C++ accessors. Lower
            # the two accessors that hostgen cannot parse to their declared
            # fields directly; avoid synthetic incomplete C01/C02/C03 tags.
            text = re.sub(
                r"([A-Za-z_]\w*)->root03\(\)",
                r"((Collider*)((PlacedObject*)\1)->root)",
                text,
            )
            text = re.sub(
                r"([A-Za-z_]\w*(?:->[A-Za-z_]\w*)*)->desc(\d+)\(\)",
                r"((const Desc\2*)((BaseObject*)(\1))->desc)",
                text,
            )
            text = re.sub(
                r"(\([^;{}]+\))->desc(\d+)\(\)",
                r"((const Desc\2*)((BaseObject*)\1)->desc)",
                text,
            )
            # One matched C unit uses a tiny local C++ iterator with a
            # constructor/destructor. Lower that iterator to its field layout
            # and initialize the fields at its sole call site; the host C
            # translation does not need its destructor bookkeeping.
            if cpp.stem == "00138000":
                text = re.sub(
                    r"struct Frame_13B610\s*\{.*?\n\};",
                    "struct Frame_13B610 { void* base; void* cur; s32 off; "
                    "struct Frame_13B610* prev; };",
                    text,
                    count=1,
                    flags=re.DOTALL,
                )
                text = text.replace(
                    "Frame_13B610 f((List_13B610*)&self->children);",
                    "Frame_13B610 f;\n"
                    "    f.base = (List_13B610*)&self->children;\n"
                    "    f.off = ((List_13B610*)f.base)->off;",
                )
            (out_dir / (cpp.stem + ".c")).write_text(prelude + text, encoding="utf-8")

    # Port-owned functional replacements stay outside sly1-decomp. Hostgen
    # treats them as ordinary C units so matching source can replace them by
    # removing the corresponding backfill file.
    backfills = ROOT / "port" / "game" / "sly1-us" / "hand" / "native_c"
    if backfills.is_dir():
        out_dir = staged / "src" / "native_backfills"
        out_dir.mkdir()
        for source_file in sorted(backfills.rglob("*")):
            if source_file.is_file() and source_file.suffix in (".c", ".h"):
                target = out_dir / source_file.relative_to(backfills)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source_file, target)
    return staged


def run_hostgen(source, version, asm):
    """Prepare and translate the available matched C into the ignored build tree."""
    if version != "us":
        raise ValueError("hostgen is configured for NTSC-U only")
    staged = prepare_c_view(source, version)
    config = ROOT / "port" / "game" / "sly1-us" / "hostgen.json"
    output_name = version if asm == "off" else version + "-asm-research"
    out = ROOT / "build" / "port" / "hostgen" / output_name
    if asm == "off":
        out.mkdir(parents=True, exist_ok=True)
        for generated_asm in out.glob("asm_*.c"):
            generated_asm.unlink()
    cmd = [sys.executable, str(ROOT / "port/tools/hostgen/hostgen.py"),
           "--game", str(config), "--source", str(staged), "--out", str(out), "--jobs", "1",
           "--asm", asm, "--allow-unreadable"]
    if asm == "on":
        cmd += ["--asm-source", str(source)]
        retail = source / "baserom" / version / "SCUS_971.98"
        if retail.is_file():
            cmd += ["--retail-exe", str(retail)]
    proc = subprocess.run(cmd, cwd=ROOT)
    return proc.returncode


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("status", help="report decompilation inputs and current code coverage")
    p.add_argument("--version", choices=("us", "eu"), default="us")
    p.add_argument("--source", help="sly1-decomp checkout (or OPENTHIEVIUS_SOURCE_SLY1)")
    p.add_argument("--json", action="store_true", help="print machine-readable JSON")
    hp = sub.add_parser("hostgen", help="generate host-side C for matched NTSC-U sources")
    hp.add_argument("--source", help="sly1-decomp checkout (or OPENTHIEVIUS_SOURCE_SLY1)")
    hp.add_argument("--asm", choices=("off", "on"), default="off",
                    help="translate unmatched retail assembly (default: off; native builds must not execute it)")
    args = parser.parse_args(argv)
    source = source_root(args.source)
    if args.command == "hostgen":
        if not (source / "config/us/functions.tsv").is_file():
            print("error: no sly1-decomp checkout at %s" % source, file=sys.stderr)
            return 1
        return run_hostgen(source, "us", args.asm)
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

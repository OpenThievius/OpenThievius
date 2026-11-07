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
                # C++ permits these derived fields to hide inherited fields.
                # Anonymous C aggregates expose both names in one scope. Rename
                # only the inherited copy: derived accesses retain C++ lookup,
                # and explicit base casts still use the unchanged base type.
                # Storage and retail offsets must remain intact.
                hidden_fields = {"Animation": ("id",),
                                 "ObjectCycler": ("index",)}
                for field in hidden_fields.get(tag, ()):
                    body = re.sub(r"\b" + field + r"\b",
                                  "sly_base_" + base + "_" + field, body)
                flattened.add(tag)
                changed = True
                return match.group(1) + " {\n    struct {" + body + "\n    };"
            header_texts[header] = inheritance.sub(inline_base, header_text)
        ready.update(flattened)
        if not changed:
            break

    staged_bases = struct_bodies("\n".join(header_texts.values()))
    staged_bases.update(struct_bodies("\n".join(
        path.read_text(encoding="utf-8") for directory in ("main", "lib")
        for path in sorted((source / "src" / directory).glob("*.cpp")))))

    screen_header = next(
        (header for header in header_texts if header.name == "screen_element.h"), None
    )
    if screen_header is not None:
        header_text = header_texts[screen_header]
        declaration = re.search(r"struct T217D_0F0\s*\{", header_text)
        if declaration is not None:
            depth = 1
            pos = declaration.end()
            while pos < len(header_text) and depth:
                if header_text[pos] == "{":
                    depth += 1
                elif header_text[pos] == "}":
                    depth -= 1
                pos += 1
            body = header_text[declaration.start():pos]
            body = re.sub(r"\bf32\s+scale\s*;", "f32 textScale;", body, count=1)
            header_texts[screen_header] = (
                header_text[:declaration.start()] + body + header_text[pos:]
            )

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
            # Lower source-local single inheritance to an anonymous base
            # aggregate, matching the header staging while preserving offsets.
            # Resolve only bases whose full body is available in this source
            # snapshot; leave unknown layouts untouched for Clang to reject.
            for _ in range(8):
                changed = False
                def inline_source_base(match):
                    nonlocal changed
                    base = match.group(2)
                    body = staged_bases.get(base)
                    if body is None:
                        return match.group(0)
                    changed = True
                    return match.group(1) + " {\n    struct {" + body + "\n    };"
                text = inheritance.sub(inline_source_base, text)
                if not changed:
                    break
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
            # Lower only complete one-method static helper structs. Their
            # explicit arguments are already independent of `this`; the
            # conservative grammar excludes methods, data fields and nested
            # class state so their C storage/ABI is unchanged.
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
                if tail is None:
                    # Only a complete, single-method helper can become a
                    # free function. Keep multi-member classes intact.
                    break
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

            # Lower the two simple record methods used by the memory-card
            # paths in 0018C000. Their bodies use only explicit fields and
            # calls, so adding the record pointer as argument zero preserves
            # the target layout and gives C the same `this` value.
            if cpp.stem == "file_1E08F8":
                def lower_record_method(tag, method_pattern, helper_name,
                                        helper_signature, fields=()):
                    nonlocal text
                    struct = re.search(r"struct\s+" + tag + r"\s*\{", text)
                    if struct is None:
                        raise ValueError(f"file_1E08F8: missing {tag}")
                    method = re.search(method_pattern, text[struct.end():])
                    if method is None:
                        raise ValueError(f"file_1E08F8: missing {tag} method")
                    start = struct.end() + method.start()
                    body_open = struct.end() + method.end() - 1
                    depth, pos = 1, body_open + 1
                    while pos < len(text) and depth:
                        if text[pos] == "{": depth += 1
                        elif text[pos] == "}": depth -= 1
                        pos += 1
                    if depth:
                        raise ValueError(f"file_1E08F8: unclosed {tag} method")
                    body = text[body_open + 1:pos - 1]
                    body = re.sub(r"\bthis\b", "self", body)
                    for field in fields:
                        body = re.sub(r"\b" + field + r"\b", "self->" + field, body)
                    text = text[:start] + text[pos:]
                    struct = re.search(r"struct\s+" + tag + r"\s*\{", text)
                    depth, pos = 1, struct.end()
                    while pos < len(text) and depth:
                        if text[pos] == "{": depth += 1
                        elif text[pos] == "}": depth -= 1
                        pos += 1
                    if depth:
                        raise ValueError(f"file_1E08F8: unclosed {tag} record")
                    if pos < len(text) and text[pos] == ";":
                        pos += 1
                    return_type, arguments = helper_signature.split("(", 1)
                    helper = ("\nstatic " + return_type + " " + helper_name
                              + "(" + arguments + " {" + body + "\n}\n")
                    text = text[:pos] + helper + text[pos:]

                lower_record_method(
                    "Ctx_1E0CD0", r"void\s+Pad\s*\(u8\s+ch,\s*s32\s+count\)\s*\{",
                    "Ctx_1E0CD0_Pad", "void(struct Ctx_1E0CD0* self, u8 ch, s32 count)")
                if "ctx->Pad(pad, left)" not in text or "ctx->Pad(0x20, right)" not in text:
                    raise ValueError("file_1E08F8: Ctx::Pad call sites changed")
                text = text.replace("ctx->Pad(pad, left)",
                                    "Ctx_1E0CD0_Pad(ctx, pad, left)", 1)
                text = text.replace("ctx->Pad(0x20, right)",
                                    "Ctx_1E0CD0_Pad(ctx, 0x20, right)", 1)

                lower_record_method(
                    "Sub_1E4EA8", r"void\s+SetOwner\s*\(void\*\s+o\)\s*\{",
                    "Sub_1E4EA8_SetOwner",
                    "void(struct Sub_1E4EA8* self, void* o)", fields=("owner",))
                if "sub.SetOwner(&o30)" not in text:
                    raise ValueError("file_1E08F8: Sub::SetOwner call site changed")
                text = text.replace("sub.SetOwner(&o30)",
                                    "Sub_1E4EA8_SetOwner(&sub, &o30)", 1)

                ctor_decl = "    Ctor_1E4E38();"
                if ctor_decl not in text:
                    raise ValueError("file_1E08F8: constructor declaration changed")
                text = text.replace(ctor_decl, "", 1)
                ctor = re.search(
                    r"Ctor_1E4E38::Ctor_1E4E38\s*\(\)\s*\{(?P<body>[^{}]*)\}",
                    text,
                )
                if ctor is None:
                    raise ValueError("file_1E08F8: constructor body changed")
                body = re.sub(r"\b(f4|f8|fC|f10|body)\b", r"self->\1",
                              ctor.group("body"))
                body = re.sub(
                    r"(func_001F5C0C\(self->body, 0, 0x20\);)",
                    r"\1\n    return self;",
                    body,
                    count=1,
                )
                if "return self;" not in body:
                    raise ValueError("file_1E08F8: constructor return shape changed")
                text = (text[:ctor.start()] +
                        "Ctor_1E4E38* func_001E4E38(Ctor_1E4E38* self) {" + body + "\n}" +
                        text[ctor.end():])

                double_decl = "double func_001FCDE0(double a, double b);"
                int_decl = "s32 func_001FD410(double a);"
                if double_decl not in text or int_decl not in text:
                    raise ValueError("file_1E08F8: soft-float declarations changed")
                text = text.replace(
                    double_decl,
                    'double func_001FCDE0__double(double a, double b) '
                    '__asm__("func_001FCDE0");', 1)
                text = text.replace(int_decl,
                    's32 func_001FD410__double(double a) '
                    '__asm__("func_001FD410");', 1)
                text = text.replace("i = func_001FD410(l);",
                                    "i = func_001FD410__double(l);", 1)
                old_round = "func_001FCDE0(func_001FD868(p), 0.5)"
                if old_round not in text:
                    raise ValueError("file_1E08F8: double rounding call changed")
                text = text.replace(old_round,
                                    "func_001FCDE0__double(func_001FD868(p), 0.5)", 1)

            if cpp.stem == "file_18D8E0":
                prelude += (
                    "typedef struct Sink_18E6C0 Sink_18E6C0;\n"
                    "typedef struct Ring_18E6C0 Ring_18E6C0;\n"
                    "typedef struct Dma_18E8C8 Dma_18E8C8;\n"
                )
                class_replacements = (
                    (r"class\s+Sink_18E6C0\s*\{.*?\};",
                     "struct Sink_18E6C0 { u32 vtable; };"),
                    (r"class\s+Ring_18E6C0\s*:\s*public\s+Sink_18E6C0\s*\{.*?\};",
                     "struct Ring_18E6C0 { u32 vtable; s32 f04; s32 f08; "
                     "s32 f0C; s32 f10; s32 f14; s32 f18; s32 f1C; };"),
                    (r"class\s+Dma_18E8C8\s*:\s*public\s+Sink_18E6C0\s*\{.*?\};",
                     "struct Dma_18E8C8 { u32 vtable; };"),
                )
                for pattern, replacement in class_replacements:
                    text, count = re.subn(pattern, replacement, text, count=1,
                                          flags=re.DOTALL)
                    if count != 1:
                        raise ValueError("file_18D8E0: movie sink class shape changed")

                def lower_qualified_method(pattern, signature, fields=()):
                    nonlocal text
                    method = re.search(pattern, text)
                    if method is None:
                        raise ValueError("file_18D8E0: movie sink method shape changed")
                    body_open = method.end() - 1
                    depth, pos = 1, body_open + 1
                    while pos < len(text) and depth:
                        if text[pos] == "{": depth += 1
                        elif text[pos] == "}": depth -= 1
                        pos += 1
                    if depth:
                        raise ValueError("file_18D8E0: unclosed movie sink method")
                    body = re.sub(r"\bthis\b", "self", text[body_open + 1:pos - 1])
                    for field in fields:
                        body = re.sub(r"(?<![A-Za-z0-9_>.])\b" + field + r"\b",
                                      "self->" + field, body)
                    text = text[:method.start()] + signature + " {" + body + "}" + text[pos:]

                lower_qualified_method(
                    r"s32\s+Ring_18E6C0::Put\s*\(s32\s+count,\s*u8\*\s+dst\)\s*\{",
                    "s32 func_0018E6C0(Ring_18E6C0* self, s32 count, u8* dst)",
                    fields=("f04", "f08", "f0C", "f10", "f14", "f18", "f1C"))
                lower_qualified_method(
                    r"s32\s+Ring_18E6C0::Idle\s*\(\)\s*\{",
                    "s32 func_0018E8C0(Ring_18E6C0* self)")
                lower_qualified_method(
                    r"s32\s+Dma_18E8C8::Put\s*\(s32\s+size,\s*u8\*\s+buf\)\s*\{",
                    "s32 func_0018E8C8(Dma_18E8C8* self, s32 size, u8* buf)")
                lower_qualified_method(
                    r"s32\s+Dma_18E8C8::Idle\s*\(\)\s*\{",
                    "s32 func_0018E938(Dma_18E8C8* self)")

                dma_init = "        Dma_18E8C8 dma;"
                if dma_init not in text:
                    raise ValueError("file_18D8E0: DMA stack object site changed")
                text = text.replace(dma_init,
                                    dma_init + "\n        dma.vtable = 0x0024B598;", 1)
                ring_setup = (
                    "    SetIopVolumeChannel(8, 0x200);\n"
                    "    r = OpenIopRing(0x6000, 0x400, 0, 2, 2);"
                )
                if ring_setup not in text:
                    raise ValueError("file_18D8E0: ring sink setup changed")
                text = text.replace(
                    ring_setup,
                    "    SetIopVolumeChannel(8, 0x200);\n"
                    "    self->sub6C.vtable = 0x0024B5B8;\n"
                    "    r = OpenIopRing(0x6000, 0x400, 0, 2, 2);",
                    1,
                )
                ring_init = "        InitIopRingSink(&self->sub6C, 0x6000, r);"
                if ring_init not in text:
                    raise ValueError("file_18D8E0: ring sink initialization changed")

            if cpp.stem == "file_112698":
                for declaration in ("    Gc_259F38();", "    ~Gc_259F38();"):
                    if declaration not in text:
                        raise ValueError("file_112698: collector declaration changed")
                    text = text.replace(declaration, "", 1)
                ctor = "Gc_259F38::Gc_259F38()\n{\n}\n"
                dtor = "Gc_259F38::~Gc_259F38()\n{\n}\n"
                if ctor not in text or dtor not in text:
                    raise ValueError("file_112698: collector constructor shape changed")
                text = text.replace(
                    ctor,
                    "Gc_259F38* func_0011A678(Gc_259F38* self) { return self; }\n",
                    1,
                )
                text = text.replace(
                    dtor,
                    "void func_0011A680(Gc_259F38* self, s32 deleting) { "
                    "if (deleting & 1) func_0018D778(self); }\n",
                    1,
                )
                ret_methods = (
                    "    RetValue() { func_0011B008(this); }\n"
                    "    RetValue(const RetValue& o) { func_0011B018(this, &o); }\n"
                    "    ~RetValue() { func_0011B060(this, 2); }"
                )
                if ret_methods not in text:
                    raise ValueError("file_112698: RetValue method shape changed")
                text = text.replace(ret_methods, "", 1)
                for name, old, new in (
                    ("func_00118198",
                     "RetValue func_00118198(s32 id, s32 count, ScriptValue* args, R11A630* scope)",
                     "ScriptValue* func_00118198(ScriptValue* ret, s32 id, s32 count, ScriptValue* args, R11A630* scope)"),
                    ("func_00119218",
                     "RetValue func_00119218(R11B008Node* expr, R11A630* scope)",
                     "ScriptValue* func_00119218(ScriptValue* ret, R11B008Node* expr, R11A630* scope)"),
                ):
                    if old not in text:
                        raise ValueError(f"file_112698: {name} return signature changed")
                    text = text.replace(old, new, 1)
                for old, new in (("RetValue l0;", "ScriptValue l0;\n    func_0011B008(&l0);"),
                                 ("RetValue L;", "ScriptValue L;\n    func_0011B008(&L);")):
                    if old not in text:
                        raise ValueError("file_112698: RetValue local changed")
                    text = text.replace(old, new, 1)
                for local in ("l0", "L"):
                    pattern = re.compile(r"^(\s*)return " + local + r";$", re.MULTILINE)
                    replacement = ("\\1func_0011B018(ret, &" + local + ");\n"
                                   "\\1func_0011B060(&" + local + ", 2);\n"
                                   "\\1return ret;")
                    text, count = pattern.subn(replacement, text)
                    if count != 2:
                        raise ValueError(f"file_112698: expected two {local} value returns")

            if cpp.stem == "0011C000":
                # The retail pool helpers pass their incoming a0 through the
                # leaf at 0011C3C0. The checked-in C view dropped that implicit
                # argument from both the helper declaration and its 0011C3C8
                # caller. Restore the verified retail ABI in this staged copy.
                for old, new in (
                    ("Cell_11C3C8* func_0011C3C0(void);",
                     "Cell_11C3C8* func_0011C3C0(u8* base);"),
                    ("s32 func_0011C3C8(void)\n{\n    return GetPoolEntryHeader()->value == 0;\n}",
                     "s32 func_0011C3C8(u8* base)\n{\n    return GetPoolEntryHeader(base)->value == 0;\n}"),
                ):
                    if old not in text:
                        raise ValueError("0011C000: pool helper ABI shape changed")
                    text = text.replace(old, new, 1)

                # Retail 0011FFA8 tail-calls the void 0011F6B0 worker and
                # returns its final v0. The worker's fixed three-iteration
                # loop leaves v0=0 at return; the staged helper declaration
                # incorrectly made this a void expression. Keep the wrapper's
                # retail return value without changing the shared worker ABI.
                old = (
                    "void* func_0011FFA8(T2196_6E8* self, u32 a1, u32 a2, u32 a3, u32 a4)\n"
                    "{\n    return func_0011F6B0(self->axis, a1, a2, &self->base, a3, a4);\n}"
                )
                new = (
                    "void* func_0011FFA8(T2196_6E8* self, u32 a1, u32 a2, u32 a3, u32 a4)\n"
                    "{\n    func_0011F6B0(self->axis, a1, a2, &self->base, a3, a4);\n    return 0;\n}"
                )
                if old not in text:
                    raise ValueError("0011C000: rotation wrapper ABI shape changed")
                text = text.replace(old, new, 1)

            if cpp.stem == "0018C000":
                for tag, method, args, call in (
                    ("Pair_18CC20", "Set", "Key_18CC20* key", "pair.Set(key)"),
                    ("Key_18F198", "Same", "Key_18F198* b", "a->Same(b)"),
                    ("Pair_18F198", "Set", "Key_18F198* key", "pair.Set(key)"),
                ):
                    pattern = (r"struct\s+" + tag + r"\s*\{(?P<head>.*?)"
                               r"\bs32\s+" + method + r"\s*\([^{}]*\)\s*\{"
                               r"(?P<body>.*?)\n\s*\}\s*\};")
                    match = re.search(pattern, text, flags=re.DOTALL)
                    if match is None:
                        continue
                    cname = tag + "_" + method
                    body = match.group("body").replace("this", "self")
                    if tag == "Pair_18CC20":
                        body = re.sub(r"\b(first|second)\b", r"self->\1", body)
                    elif tag == "Pair_18F198":
                        body = re.sub(r"\bk\b", "self->k", body)
                    replacement = ("struct " + tag + " {" + match.group("head") + "};\n"
                                   "static s32 " + cname + "(struct " + tag + "* self, " + args +
                                   ") {" + body + "\n}")
                    text = text[:match.start()] + replacement + text[match.end():]
                    text = text.replace(call, cname + "(&" + call.split(".")[0] + ", " +
                                        args.split()[-1].split("*")[-1] + ")")

                # Retail routines return FinishMemoryCardCall's status in v0.
                # The current C bodies discard that value and are declared
                # void, but their callers consume it as a status code. Correct
                # only the staged declarations/definitions to the verified
                # retail return type.
                for name in ("func_0018D328", "func_0018D2B0"):
                    text = re.sub(r"\bvoid\s+(" + name + r"\s*\()",
                                  r"s32 \1", text)
                    body = re.compile(
                        r"(s32\s+" + name + r"\s*\([^)]*\)\s*\{)(.*?)(\n\})",
                        flags=re.DOTALL,
                    )
                    text, count = body.subn(
                        lambda match: match.group(1) + re.sub(
                            r"(\s*)FinishMemoryCardCall\(([^;]+)\);",
                            r"\1return FinishMemoryCardCall(\2);",
                            match.group(2),
                            count=1,
                        ) + match.group(3),
                        text,
                        count=1,
                    )
                    if count != 1:
                        raise ValueError(f"{cpp.stem}: could not stage {name} status return")

            if cpp.stem == "00128000":
                # The decomp has several range-record setters whose retail
                # ABI takes the PlacedObject in a0 and a float in f12. Some
                # reconstructed prototypes dropped `self` or accidentally
                # added the float to EnsureRangeRecord itself. Repair only
                # this staged unit; keep the upstream decomp source intact.
                text = re.sub(
                    r"void\*\s+func_001291D0\s*\([^{};]*\)\s*;",
                    "void* func_001291D0(PlacedObject* self);",
                    text,
                )
                for name in ("func_001292E8", "func_00129318",
                             "func_00129348", "func_00129378"):
                    text = re.sub(
                        r"void\s+" + name + r"\s*\(\s*f32\s+x\s*\)",
                        "void " + name + "(PlacedObject* self, f32 x)",
                        text,
                    )
                text = text.replace("EnsureRangeRecord(self, x)",
                                    "EnsureRangeRecord(self)")
                text = text.replace("EnsureRangeRecord(x)",
                                    "EnsureRangeRecord(self)")
                method = re.search(
                    r"struct\s+St_12B2F8\s*\{(?P<head>.*?)"
                    r"s32\s+Set\s*\(s32\s+v\)\s*\{(?P<body>[^{}]*)\}\s*\};",
                    text,
                    flags=re.DOTALL,
                )
                if method is None:
                    raise ValueError("00128000: St_12B2F8::Set shape changed")
                body = re.sub(r"\bf13\b", "self->f13", method.group("body"))
                replacement = (
                    "struct St_12B2F8 {" + method.group("head") + "};\n"
                    "static s32 St_12B2F8_Set(struct St_12B2F8* self, s32 v) {"
                    + body + "\n}"
                )
                text = text[:method.start()] + replacement + text[method.end():]
                for old, new in (
                    ("st->Set(-1)", "St_12B2F8_Set(st, -1)"),
                    ("((St_12B2F8*)self->f2C0)->Set(3)",
                     "St_12B2F8_Set((St_12B2F8*)self->f2C0, 3)"),
                ):
                    if old not in text:
                        raise ValueError("00128000: St_12B2F8::Set call site changed")
                    text = text.replace(old, new, 1)

            if cpp.stem in {"00134000", "00180000", "001DC000", "file_1963D8"}:
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
            if cpp.stem == "00124000":
                # TargetBinding is the base subobject at offset zero. The
                # anonymous C aggregate already contains its fields; lower
                # this local derived record while retaining its retail size.
                text = text.replace(
                    "struct Frame_124CC0 : TargetBinding { u8 pad0[8]; f32 tmp; };",
                    "struct Frame_124CC0 { s32 key; void* object; "
                    "u8 pad0[8]; f32 tmp; };",
                )
                mixed_decl = (
                    "void func_001BFCD8(s32 a0, f32 f12, f32 f13, f32 f14, "
                    "f32 f15, f32 f16, void* a1, PlacedObject* a2, s32 a3, "
                    "void* a4, s32 a5);"
                )
                mixed_alias = (
                    "void func_001BFCD8__mixed(s32 a0, f32 f12, f32 f13, "
                    "f32 f14, f32 f15, f32 f16, void* a1, PlacedObject* a2, "
                    "s32 a3, void* a4, s32 a5) __asm__(\"func_001BFCD8\");"
                )
                if mixed_decl not in text:
                    raise ValueError("00124000 sound call signature changed")
                text = text.replace(mixed_decl, mixed_alias, 1)
                sound_call = "StartSoundEffect(q->f00, q->f04, q->f08, q->f0C, q->f10, q->f20, &q->f1C, self, 0, q->b14, 0);"
                if sound_call not in text:
                    raise ValueError("00124000 mixed sound call changed")
                text = text.replace(sound_call, sound_call.replace(
                    "StartSoundEffect(", "func_001BFCD8__mixed("), 1)
            if cpp.stem == "file_1963D8":
                text = text.replace(
                    "void* func_001925F0(void);",
                    "void* func_001925F0(void*, f32);",
                )
            if cpp.stem == "00148000":
                # SN's target ABI calls this 16-byte aggregate through an
                # output pointer. The decomp's C++ return form makes the host
                # frontend emit a different call convention, so lower just
                # this definition to the equivalent target-style sret form
                # in disposable staging. Matching decomp sources stay intact.
                text = text.replace(
                    "s32 func_001493C8(void* out, T2173_3B0* self);",
                    "void func_001493C8(void* out, T2173_3B0* self);",
                )
                text = text.replace(
                    "Quad_1493C8 func_001493C8(T2173_3B0* self)",
                    "void func_001493C8(void* out, T2173_3B0* self)",
                )
                text = text.replace(
                    "    return r;\n}",
                    "    *(Quad_1493C8*)out = r;\n}\n",
                    1,
                )
            # One matched C unit uses a tiny local C++ iterator with a
            # constructor/destructor. Lower that iterator to its field layout
            # and initialize the fields at its sole call site; the host C
            # translation does not need its destructor bookkeeping.
            if cpp.stem == "001DC000":
                text = re.sub(
                    r"s32 differs\(\)\s*\{\s*if \(this == 0\) return 0;\s*return a != b;\s*\}",
                    "",
                    text,
                )
                text = text.replace(
                    "((Cur_11A338*)g_currentWorld->f2310)->differs()",
                    "Cur_11A338_differs((Cur_11A338*)g_currentWorld->f2310)",
                )
                cursor_decl = re.search(r"struct Cur_11A338\s*\{.*?\};", text, flags=re.DOTALL)
                if cursor_decl is not None:
                    helper = (
                        "\nstatic s32 Cur_11A338_differs(struct Cur_11A338* self) {\n"
                        "    if (self == 0) return 0;\n"
                        "    return self->a != self->b;\n}\n"
                    )
                    text = text[:cursor_decl.end()] + helper + text[cursor_decl.end():]
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


def run_hostgen(source, version, asm, native_asm=False, asm_source=None):
    """Prepare and translate the available matched C into the ignored build tree."""
    if version != "us":
        raise ValueError("hostgen is configured for NTSC-U only")
    staged = prepare_c_view(source, version)
    config = ROOT / "port" / "game" / "sly1-us" / "hostgen.json"
    output_name = version if asm == "off" or native_asm else version + "-asm-research"
    out = ROOT / "build" / "port" / "hostgen" / output_name
    if asm == "off":
        out.mkdir(parents=True, exist_ok=True)
        for generated_asm in out.glob("asm_*.c"):
            generated_asm.unlink()
    cmd = [sys.executable, str(ROOT / "port/tools/hostgen/hostgen.py"),
           "--game", str(config), "--source", str(staged), "--out", str(out), "--jobs", "1",
           "--asm", asm, "--allow-unreadable"]
    if asm == "on":
        cmd += ["--asm-source", str(asm_source or source)]
        retail = source / "baserom" / version / "SCUS_971.98"
        if retail.is_file():
            cmd += ["--retail-exe", str(retail)]
    proc = subprocess.run(cmd, cwd=ROOT)
    if proc.returncode != 0:
        return proc.returncode
    if asm == "on" and native_asm:
        report = json.loads((out / "report.json").read_text())
        required = set(json.loads(config.read_text()).get("asm", {}).get("only", []))
        translated = set(report.get("from_assembly", {}))
        missing = sorted(required - translated)
        if missing:
            print("error: selected retail assembly was not translated: " + ", ".join(missing)
                  + "; set --asm-source to a checkout with asm/us/text.s", file=sys.stderr)
            return 1
    return 0


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
                    help="translate unmatched retail assembly (default: off)")
    hp.add_argument("--native-asm", action="store_true",
                    help="put selected assembly translations in the native build output")
    hp.add_argument("--asm-source", help="checkout with the US retail assembly files")
    args = parser.parse_args(argv)
    source = source_root(args.source)
    if args.command == "hostgen":
        if not (source / "config/us/functions.tsv").is_file():
            print("error: no sly1-decomp checkout at %s" % source, file=sys.stderr)
            return 1
        return run_hostgen(source, "us", args.asm, args.native_asm, args.asm_source)
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

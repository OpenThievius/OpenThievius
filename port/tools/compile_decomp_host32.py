#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Compile Sly decompilation C++ directly for a native 32-bit host ABI."""

import argparse
import re
import shutil
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
    parser.add_argument("--trace-functions", action="store_true",
                        help="add targeted startup state traces without instrumenting game calls")
    args = parser.parse_args(argv)

    source = args.source.expanduser().resolve()
    out = args.out.expanduser().resolve()
    port = args.port.resolve()
    include = source / "include"
    names_header = include / "names.h"
    # This launcher targets NTSC-U. A proven whole-file unit replaces only its
    # own address range, not an entire 0x4000 window that happens to overlap it.
    # Keep chunks with uncovered text so matching functions after a file's end
    # are not silently dropped from the native build.
    whole_files = []
    whole_ranges = []
    files_table = source / "config" / "us" / "files.tsv"
    if files_table.is_file():
        for row in files_table.read_text(encoding="utf-8").splitlines()[1:]:
            fields = row.split("\t")
            if len(fields) < 3:
                continue
            unit = source / fields[0]
            if unit.is_file():
                whole_files.append(unit)
                whole_ranges.append((int(fields[1], 16), int(fields[2], 16)))
    main = source / "src" / "main"
    chunks = []
    if main.is_dir():
        for path in main.glob("[0-9A-Fa-f]*.cpp"):
            try:
                address = int(path.stem, 16)
            except ValueError:
                continue
            window_end = address + 0x4000
            fully_covered = any(start <= address and window_end <= end
                                for start, end in whole_ranges)
            if not fully_covered:
                chunks.append(path)
    sources = sorted(set(whole_files + chunks + [
        path
        for directory in ("lib", "libm")
        for path in (source / "src" / directory).rglob("*.cpp")
    ]))
    if not sources:
        parser.error("no C++ source units found under " + str(source / "src"))
    compiler = shlex.split(args.cxx)
    failures = []
    warnings = 0
    staged_root = out / "staged"
    objects_root = out / "objects"
    shutil.rmtree(staged_root, ignore_errors=True)
    shutil.rmtree(objects_root, ignore_errors=True)
    staged_root.mkdir(parents=True, exist_ok=True)
    objects_root.mkdir(parents=True, exist_ok=True)
    backfill_root = port / "game" / "sly1-us" / "native"
    # Native backfills take precedence at their retail address. The current
    # decomp groups functions into named whole-file units, so the old
    # address-named staging rules cannot reliably find every matching body.
    # Rename only matching definitions in the staged copy; calls still use
    # the public names and therefore resolve to the port boundary.
    backfill_addresses = set()
    if backfill_root.exists():
        for path in backfill_root.rglob("*.cpp"):
            backfill_addresses.update(
                address.lower()
                for address in re.findall(r"\bfunc_([0-9A-Fa-f]{8})\s*\(",
                                          path.read_text(encoding="utf-8")))
    address_names = {
        address.lower(): name
        for name, address in re.findall(
            r"(?m)^#define\s+(\w+)\s+func_([0-9A-Fa-f]{8})\s*$",
            names_header.read_text(encoding="utf-8") if names_header.is_file() else "")
    }
    host_include = port / "sly1" / "host32" / "include"
    flags = [
        "-m32", "-msse2", "-std=gnu++20", "-fno-exceptions", "-fno-rtti",
        "-O2",
        "-fno-pie", "-fno-pic", "-ffunction-sections", "-fdata-sections",
        "-fno-asynchronous-unwind-tables", "-fno-unwind-tables", "-fpermissive",
        "-include", str(port / "sly1" / "host_abi.h"),
        "-I", str(host_include),
        "-I", str(include),
    ]
    # host_abi.h defines the retail common.h guard because it supplies the
    # host-width integer aliases. Newer decomp snapshots include generated
    # readable symbol macros from common.h, so force that independent header
    # after the ABI header rather than letting the guard suppress it.
    if names_header.is_file():
        flags[flags.index("-I"):flags.index("-I")] = ["-include", str(names_header)]
    out.mkdir(parents=True, exist_ok=True)

    for src in sources:
        rel = src.relative_to(source / "src")
        staged = staged_root / rel
        obj = objects_root / rel.with_suffix(".o")
        staged.parent.mkdir(parents=True, exist_ok=True)
        obj.parent.mkdir(parents=True, exist_ok=True)
        original = src.read_text(encoding="utf-8")
        # The PS2 compiler treats unsigned long literals as 64-bit; i386 does not.
        adapted = re.sub(r"(?i)(0x[0-9a-f]+|[0-9]+)UL\b", r"\1ULL", original)
        # A pair of format strings in the US source contains literal newlines
        # between quotes; preserve their intended newline in host C++ syntax.
        adapted = re.sub(r'"([^"\\\n]*)\n"', lambda m: '"' + m.group(1) + r'\n"', adapted)
        # func_001612F8 writes a GIF packet from two 64-bit tag arguments.
        # Several matching translation units declared those arguments as
        # 32-bit values, which shifts the final output pointer on the i386
        # host ABI. Normalize only staged prototypes; leave decomp source
        # untouched and retain each unit's first pointer type.
        adapted = re.sub(
            r"(?m)^(void func_001612F8\([^,]+),[^;\n]*;",
            r"\1, u64 prim, s32 nloop, s32 nreg, u64 data, s32* out);",
            adapted)
        if rel.as_posix() == "main/0015C000.cpp":
            # The PS2 ABI gives long 64 bits; i386 gets the same aliases from
            # host_abi.h, where they are represented with long long.
            adapted = adapted.replace("typedef signed long s64;\n", "")
            adapted = adapted.replace("typedef unsigned long u64;\n", "")
            # The display worker publishes its current packet through a
            # shared global, then dereferences it twice while the main thread
            # may clear that global during a synchronous frame handoff. Keep
            # this iteration's pointer local, while preserving the published
            # pointer for the matching consumer.
            old_frame = "        D_0026231C = &D_00262320[D_00262314];"
            new_frame = (
                "        G262320* frame = &D_00262320[D_00262314];\n"
                "        D_0026231C = frame;"
            )
            if adapted.count(old_frame) != 1:
                raise RuntimeError("expected one display frame publication in " + rel.as_posix())
            adapted = adapted.replace(old_frame, new_frame)
            for member in ("i0C", "i04"):
                old_use = "((G262320*)D_0026231C)->" + member
                if adapted.count(old_use) != 1:
                    raise RuntimeError("expected one display frame read " + member +
                                       " in " + rel.as_posix())
                adapted = adapted.replace(old_use, "frame->" + member)
            # The memory-card cleanup iterates an EE list containing valid
            # low-address nodes. Translate those pointers to the host's
            # mapped low-memory shadow before dereferencing them.
            old = (
                "        Node_15EFE8* obj = *(Node_15EFE8**)it->link;\n"
                "        it->link = (u8*)obj + it->offset;\n"
                "        return obj;"
            )
            new = (
                "        Node_15EFE8* obj = *(Node_15EFE8**)it->link;\n"
                "        u32 address = (u32)obj;\n"
                "        if (address != 0 && address < 0x10000)\n"
                "            obj = (Node_15EFE8*)(address + 0x10000);\n"
                "        it->link = (u8*)obj + it->offset;\n"
                "        return obj;"
            )
            if old not in adapted:
                raise RuntimeError("expected low-address list iterator in " + rel.as_posix())
            adapted = adapted.replace(old, new)
        elif rel.as_posix() == "main/001E8000.cpp":
            # Retail's trigonometry helper receives the angle in the first
            # argument slot, followed by the sine and cosine output pointers.
            # The decomp body models the mixed EE GPR/FPU argument placement
            # as (outputs, angle); on i386 that C++ signature reverses the
            # actual host stack layout used by its angle-first callers.
            old_signature = "void func_001EAD88(f32* sinOut, f32* cosOut, f32 angle)"
            new_signature = "void func_001EAD88(f32 angle, f32* sinOut, f32* cosOut)"
            if adapted.count(old_signature) != 1:
                raise RuntimeError("expected one EAD88 definition in " + rel.as_posix())
            adapted = adapted.replace(old_signature, new_signature)
        elif rel.as_posix() == "main/file_1C05D0.cpp":
            # The PS2 compiler lays each variadic argument in an 8-byte slot.
            # Native i386 callers use 4-byte slots, so stage the equivalent
            # host vararg walk without changing the decompilation source.
            old = "ap = (VaList_1C2430)__builtin_next_arg(size) - (8 - __builtin_args_info(0)) * 8;"
            if old not in adapted:
                raise RuntimeError("expected PS2 varargs expression not found in " + rel.as_posix())
            adapted = adapted.replace(old, "ap = (VaList_1C2430)__builtin_next_arg(size);")
            adapted = adapted.replace(
                "#define VA_ARG_1C2430(ap, T) (*(T*)((ap += 8) - 8))",
                "#define VA_ARG_1C2430(ap, T) (*(T*)((ap += sizeof(T)) - sizeof(T)))")
        elif rel.as_posix() == "lib/001FC000.cpp":
            # The retail library uses the SN Systems exception runtime. This
            # freestanding host launcher has no compatible unwind runtime;
            # route only its exceptional paths through Sly's registered
            # non-returning handlers. Ordinary allocation and library code is
            # left intact in staged source.
            begin = "void func_001FEC10(s32 count, void** allowed)\n{"
            end = "\n}\n\nvoid func_001FEDC8()"
            start_at = adapted.find(begin)
            end_at = adapted.find(end, start_at)
            if start_at < 0 or end_at < 0:
                raise RuntimeError("expected Sly exception handler not found in " + rel.as_posix())
            adapted = (adapted[:start_at] +
                       "void func_001FEC10(s32 count, void** allowed)\n{\n"
                       "    (void)count;\n    (void)allowed;\n    func_001FE938();\n" +
                       adapted[end_at:])
            adapted = adapted.replace("throw Fault_1FEDC8();", "func_001FE8F0();")
            adapted = adapted.replace("throw Fault_1FEE48();", "func_001FE8F0();")
            adapted = adapted.replace(" throw(Fault_1FD8A8)", "")
        elif rel.as_posix() == "lib/00200000.cpp":
            # These three matching functions are PS2 DMA/interrupt MMIO
            # boundaries. Keep their vanilla implementations in the staged
            # source for reference, but let separate port-native service
            # definitions own the callable retail addresses.
            for address in ("00201A18", "002027C8", "00202A80"):
                pattern = re.compile(r"\bfunc_" + address + r"(?=\s*\()")
                adapted, replacements = pattern.subn("decomp_func_" + address, adapted)
                if replacements != 1 and not (files_table.is_file() and replacements == 0):
                    raise RuntimeError("expected one platform function " + address +
                                       " in " + rel.as_posix())
            # The native display path replaces these two additional GS
            # register boundaries while keeping the original C bodies staged.
            for signature, address in (("void", "00201BB8"), ("s32", "00201C78")):
                pattern = re.compile(
                    r"(?m)^" + signature + r" func_" + address + r"\((?:void\* self|void \*self)\)"
                )
                adapted, replacements = pattern.subn(
                    signature + " decomp_func_" + address + "(void* self)", adapted)
                if replacements != 1 and not (files_table.is_file() and replacements == 0):
                    raise RuntimeError("expected one GS boundary definition " + address +
                                       " in " + rel.as_posix())
            # EE DMA wait routines poll CHCR until hardware clears STR. A
            # native DMA submission is synchronous, so port services own these
            # waits too; leave their matching bodies visible in staged C++.
            for address, pattern in (
                ("00202B68", r"(?m)^void func_00202B68\(DmaChan_202B68\* ch\)"),
                ("00202C38", r"(?m)^s32 func_00202C38\(DmaChan_202C38\* ch, s32 mode, s32 timeout\)"),
            ):
                signatures = {
                    "00202B68": "void decomp_func_00202B68(DmaChan_202B68* ch)",
                    "00202C38": "s32 decomp_func_00202C38(DmaChan_202C38* ch, s32 mode, s32 timeout)",
                }
                adapted, replacements = re.subn(pattern, signatures[address], adapted)
                if replacements != 1 and not (files_table.is_file() and replacements == 0):
                    raise RuntimeError("expected one DMA wait definition " + address +
                                       " in " + rel.as_posix())
            # Native CD reads complete synchronously. Preserve the matching
            # IOP status routine for reference, while the port reports its
            # host read result at this retail address.
            adapted, replacements = re.subn(
                r"(?m)^s32 func_00203D90\(void\)(?=\n\{)",
                "s32 decomp_func_00203D90(void)", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one CD status definition in " + rel.as_posix())
        elif rel.as_posix() == "main/00184000.cpp":
            # The candidate projection helper has a shifted argument list.
            # Preserve that body in staging while the port supplies the
            # argument flow derived from the retail call site.
            adapted, replacements = re.subn(
                r"(?m)^void func_001840F0\(f32 a, f32 b, f32 c, f32 d, Out_1840F0\* out0, void\* out1\)",
                "void decomp_func_001840F0(f32 a, f32 b, f32 c, f32 d, Out_1840F0* out0, void* out1)",
                adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one projection helper in " + rel.as_posix())
            # Retail Quad16 inputs are 16-byte aligned. Native retail-address
            # data symbols can be only 8-byte aligned, so keep the source body
            # staged and replace its aligned vector load in the port layer.
            adapted, replacements = re.subn(
                r"(?m)^void func_001847F0\(C51\* self, Quad_001847F0\* src\)",
                "void decomp_func_001847F0(C51* self, Quad_001847F0* src)",
                adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one camera vector setter in " + rel.as_posix())
        elif rel.as_posix() == "main/001EC000.cpp":
            # The matching packet-copy body calls the packet-size helper with
            # an incomplete prototype. Keep it staged while a port-only writer
            # reproduces the retail VIF header and payload copy.
            adapted, replacements = re.subn(
                r"(?m)^void func_001EE668\(void\* s, void\* a1, s32 size, u32\* src, void\* a4\)",
                "void decomp_func_001EE668(void* s, void* a1, s32 size, u32* src, void* a4)",
                adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one packet-copy definition in " + rel.as_posix())
        elif rel.as_posix() == "main/001E4000.cpp":
            # G275F80 is a 0x40-byte native view assembled from separate retail
            # labels. Reserve the full view in the host image so its tail cannot
            # overlap the next compiled global (D_00274118) when callbacks write it.
            adapted, replacements = re.subn(
                r"(?m)^s32 D_00275F80 = 0;$",
                "u8 D_00275F80[0x40] = {0};", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected the 4-byte D_00275F80 retail label in " +
                                   rel.as_posix())
            # func_00140D90 clears this system table through +0x42f, although
            # the decompilation exposes only its first 0x210 bytes as a label.
            # Reserve the whole native span so the clear cannot hit a neighbor.
            adapted, replacements = re.subn(
                r"(?m)^u8 D_00261420\[528\] = \{0\};$",
                "u8 D_00261420[0x430] = {0};", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected the D_00261420 retail table label in " +
                                   rel.as_posix())
            # Retail labels can expose only part of a T217D object (some methods
            # write large derived arrays beyond those label extents). Reserve
            # each complete typed object in the host image to prevent adjacent
            # compiled globals from being overwritten during native startup.
            object_table = (source / "src" / "main" / "001A8000.cpp").read_text(
                encoding="utf-8")
            table = re.search(
                r"extern void\* const D_002486B0\[38\]\s*=\s*\{(.*?)\};",
                object_table, re.S)
            if table is None:
                raise RuntimeError("expected the T217D object table in 001A8000.cpp")
            typed_header = (source / "include" / "t217d.h").read_text(encoding="utf-8")
            object_types = {
                name: type_name
                for type_name, name in re.findall(
                    r"extern\s+(T217D_[0-9A-Fa-f]+)\s+(D_002[0-9A-Fa-f]+);",
                    typed_header)
            }
            adapted = adapted.replace('#include "common.h"',
                                      '#include "common.h"\n#include "t217d.h"', 1)
            for name in re.findall(r"&\s*(D_002[0-9A-Fa-f]+)", table.group(1)):
                if name not in object_types:
                    raise RuntimeError("missing typed T217D object " + name)
                object = re.compile(r"(?m)^[^\n]*\b" + re.escape(name) +
                                    r"\b[^\n]*=\s*\{(?:0)?\};(?:\s*//[^\n]*)?$")
                match = object.search(adapted)
                if match is None:
                    raise RuntimeError("expected zeroed T217D storage for " + name)
                adapted = (adapted[:match.start()] + object_types[name] + " " +
                           name + " = {};" + adapted[match.end():])
        elif rel.as_posix() == "main/00168000.cpp":
            # Native DMA completion is synchronous, so the display setup path
            # must not wait for the PS2 GS interrupt semaphore. Keep the retail
            # body staged while the port supplies its own no-wait equivalent.
            adapted, replacements = re.subn(
                r"(?m)^void func_001683B8\(void\)(?=\s*\{)",
                "void decomp_func_001683B8(void)", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one GS completion wait in " + rel.as_posix())
        elif rel.as_posix() == "main/file_15F678.cpp":
            # Obj_161958 overlays the V24A0 packet writer with a C++ virtual
            # method, but the native host cannot use the retail buffer-end word
            # as a C++ vptr. Keep that body staged under a private name while a
            # port-only V24A0-aware implementation owns the callable address.
            adapted, replacements = re.subn(
                r"(?m)^    void func_00161958\(s32 count\);",
                "    void decomp_func_00161958(s32 count);", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected Obj_161958 declaration in " + rel.as_posix())
            adapted, replacements = re.subn(
                r"(?m)^void Obj_161958::func_00161958\(s32 count\)",
                "void Obj_161958::decomp_func_00161958(s32 count)", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected Obj_161958 method definition in " + rel.as_posix())
        elif rel.as_posix() == "main/file_184D78.cpp":
            # Retain the most recent one-based startup callback index in the
            # native launcher diagnostics. This isolates a startup spin loop
            # without instrumenting every retail function's return path.
            callback_declaration = (
                'extern "C" void sly1_native_startup_callback(unsigned int);\n\n'
                'void func_00185B38(void)\n{')
            adapted, replacements = re.subn(
                r"(?m)^void func_00185B38\(void\)\n\{",
                callback_declaration, adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected startup callback loop in " + rel.as_posix())
            adapted, replacements = re.subn(
                r"(?m)^(\s*)tbl\[i\]\(\);",
                r"\1sly1_native_startup_callback(i);\n\1tbl[i]();", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one startup callback call in " + rel.as_posix())
            # Replace only the IOP module/CDVD bootstrap at its retail address;
            # keep the original matching body in the staged source for review.
            adapted, replacements = re.subn(
                r"(?m)^void func_001858F8\(\)(?=\n\{)",
                "void decomp_func_001858F8()", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one IOP bootstrap definition in " +
                                   rel.as_posix())
            # Native DMA channels live in host memory; this wait routine must
            # recover the channel record if startup retained its retail MMIO
            # address in the shared global.
            adapted, replacements = re.subn(
                r"(?m)^void func_00185A58\(void\)(?=\n\{)",
                "void decomp_func_00185A58(void)", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one DMA wait definition in " + rel.as_posix())
            adapted, replacements = re.subn(
                r"(?m)^void func_00185AC8\(\)(?=\n\{)",
                "void decomp_func_00185AC8()", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one second DMA wait definition in " +
                                   rel.as_posix())
        elif rel.as_posix() == "main/00134000.cpp":
            # The decomp declares this wrapper void even though callers use
            # its status result and the wrapped initializer returns s32. Keep
            # the correction in the native staging layer so the decomp tree
            # remains untouched and can replace it once its signature is fixed.
            adapted, replacements = re.subn(
                r"(?m)^void func_00137328\(void\* a, void\* b, s32 c\);\n\n"
                r"void func_001373B0\(void\* a, S18D410\* pair\)\n\{\n"
                r"    func_00137328\(a, pair->p0, pair->second\);\n\}",
                "s32 func_00137328(void* a, void* b, s32 c);\n\n"
                "s32 func_001373B0(void* a, S18D410* pair)\n{\n"
                "    return func_00137328(a, pair->p0, pair->second);\n}",
                adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected mismatched resource-key wrapper in " +
                                   rel.as_posix())
        elif rel.as_posix() == "main/file_1E08F8.cpp" and args.trace_functions:
            # Capture the pending key/blob and current world when the direct
            # native startup path handles a resource request.
            hook = 'extern "C" void sly1_native_trace_resource_state(u32, u32, u32);\n'
            adapted = hook + adapted
            signature = "void func_001E4EA8(Self_1E4EA8* self)\n{"
            replacement = (
                signature + "\n"
                "    sly1_native_trace_resource_state((u32)self, (u32)self->f4, "
                "(u32)D_00275710);"
            )
            if adapted.count(signature) != 1:
                raise RuntimeError("expected one resource request handler in " + rel.as_posix())
            adapted = adapted.replace(signature, replacement)
        elif rel.as_posix() == "main/0011C000.cpp":
            # Sound RPC requests cannot be serviced without Sly's IOP sound
            # module. Keep the matching body staged while the port supplies a
            # synchronous, silent native service at this retail address.
            adapted, replacements = re.subn(
                r"(?m)^s32 func_0011D968\(s32 id, s32 size, u8\* data\)(?=\n\{)",
                "s32 decomp_func_0011D968(s32 id, s32 size, u8* data)", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one sound RPC definition in " + rel.as_posix())
            # The native port has no Sly IOP sound module. Keep the matching
            # bank loader available in staged source while the port supplies
            # a silent, synchronous implementation at its retail address.
            adapted, replacements = re.subn(
                r"(?m)^s32 func_0011D140\(s32 a0, s32 a1\)(?=\n\{)",
                "s32 decomp_func_0011D140(s32 a0, s32 a1)", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one sound bank loader definition in " + rel.as_posix())
        elif rel.as_posix() == "lib/00204000.cpp":
            # Replace the IOP RPC wait with the native CD service; retain the
            # retail body in staged source so it remains available for review.
            adapted, replacements = re.subn(
                r"(?m)^s32 func_002040E8\(void\* arg\)(?=\n\{)",
                "s32 decomp_func_002040E8(void* arg)", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one IOP service-start definition in " +
                                   rel.as_posix())
        elif (rel.as_posix() == "lib/001F8000.cpp" or
              "BindRemoteService(" in adapted or "func_001F80C0(" in adapted):
            # SIF RPC completion for the two sound bootstrap services has no
            # IOP peer in the host build. Keep the matching implementation in
            # staged source, and let a port-only synchronous boundary own the
            # retail address.
            adapted, replacements = re.subn(
                r"(?m)^s32 (?:func_001F80C0\(Ctx_1F80C0\* self, s32 a1, s32 a2\)|"
                r"BindRemoteService\(RemoteCallClient\* self, u32 cmd, s32 mode\))(?=\n\{)",
                lambda match: match.group(0).replace("func_001F80C0", "decomp_func_001F80C0")
                .replace("BindRemoteService", "decomp_BindRemoteService"), adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one SIF RPC definition in " + rel.as_posix())
        elif rel.as_posix() == "main/00150000.cpp":
            # Native processes cannot map valid EE low-memory addresses below
            # the host mmap_min_addr. Keep the matching list-offset helper in
            # the staged source and let a port-only boundary translate those
            # low results into the corresponding mapped guest-RAM shadow.
            adapted, replacements = re.subn(
                r"(?m)^u32 func_00152460\(List01\* p, u32 value\)(?=\n\{)",
                "u32 func_00152460(List01* p, u32 value);\n"
                "u32 decomp_func_00152460(List01* p, u32 value)", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one list address helper in " + rel.as_posix())
            adapted, replacements = re.subn(
                r"(?m)^void func_00152470\(List01\* self, u32 value\)(?=\n\{)",
                "void func_00152470(List01* self, u32 value);\n"
                "void decomp_func_00152470(List01* self, u32 value)", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one list append definition in " + rel.as_posix())
        elif rel.as_posix() == "main/001B4000.cpp":
            # The renderer's first cleanup can run before a world exists.
            # Keep the matching pool insertion body staged and let the native
            # boundary safely ignore returns to the absent world's pool.
            adapted, replacements = re.subn(
                r"(?m)^void func_001B5F28\(Pool6C\* list, T2195\* item\)(?=\n\{)",
                "void decomp_func_001B5F28(Pool6C* list, T2195* item)", adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one world pool insertion in " + rel.as_posix())
        elif rel.as_posix() == "main/00200000.cpp":
            # The retail DMA table contains MMIO addresses. The native port
            # owns separate host channel records, so route this lookup through
            # the port service while retaining the matching body in staging.
            adapted, replacements = re.subn(
                r"(?m)^u32 func_002027A0\(u32 index\) \{[^\n]*\}",
                "u32 decomp_func_002027A0(u32 index) { if (index >= 10) return 0; return D_00279340[index]; }",
                adapted)
            if replacements != 1 and not (files_table.is_file() and replacements == 0):
                raise RuntimeError("expected one DMA channel lookup in " + rel.as_posix())
        if args.trace_functions and rel.as_posix() in {
                "main/00124000.cpp", "main/001B4000.cpp", "main/001B8000.cpp"}:
            # Global -finstrument-functions changes behavior where the decomp
            # has mismatched prototypes. Trace only the C02 root lifecycle at
            # selected boundaries so diagnostics do not perturb game calls.
            hook = 'extern "C" void sly1_native_trace_c02_state(u32, u32);\n'
            adapted = hook + adapted
            trace_points = {
                "main/00124000.cpp": [
                    ("void func_001240F8(C02* self)\n{",
                     "    sly1_native_trace_c02_state(0x1240F8, (u32)self);\n"),
                    ("        self->root = self;",
                     "        self->root = self;\n"
                     "        sly1_native_trace_c02_state(0x1240F9, (u32)self);"),
                    ("        self->root = ((C02*)self->parent)->root;",
                     "        self->root = ((C02*)self->parent)->root;\n"
                     "        sly1_native_trace_c02_state(0x1240FA, (u32)self);")],
                "main/001B4000.cpp": [
                    ("void func_001B7968(C03* self)\n{",
                     "    sly1_native_trace_c02_state(0x1B7968, (u32)self);\n"),
                    ("void func_001B7A98(C03* self)\n{",
                     "    sly1_native_trace_c02_state(0x1B7A98, (u32)self);\n"),
                    ("    func_001240F8(self);",
                     "    func_001240F8(self);\n"
                     "    sly1_native_trace_c02_state(0x1B7A99, (u32)self);")],
                "main/001B8000.cpp": [
                    ("void func_001B89B8(C03* self)\n{",
                     "    sly1_native_trace_c02_state(0x1B89B8, (u32)self);\n"),
                    ("    func_00125108(self);",
                     "    func_00125108(self);\n"
                     "    sly1_native_trace_c02_state(0x1B89B9, (u32)self);"),
                    ("        self->desc03()->m32(self);",
                     "        sly1_native_trace_c02_state(0x1BAC30, (u32)self);\n"
                     "        self->desc03()->m32(self);")]
            }[rel.as_posix()]
            for old, new in trace_points:
                if adapted.count(old) != 1:
                    raise RuntimeError("expected one C02 trace location in " +
                                       rel.as_posix() + ": " + old.splitlines()[0])
                adapted = adapted.replace(old, old + "\n" + new, 1)
        if args.trace_functions and rel.as_posix() == "main/00180000.cpp":
            adapted = 'extern "C" void sly1_native_trace_c02_state(u32, u32);\n' + adapted
            hook = 'extern "C" void sly1_native_trace_c02_list(u32, u32, u32, u32);\n'
            adapted = hook + adapted
            signature = "void func_00182C80(C01* self)\n{"
            if adapted.count(signature) != 1:
                raise RuntimeError("expected C01 parent-link constructor in " + rel.as_posix())
            adapted = adapted.replace(
                signature,
                signature + "\n    sly1_native_trace_c02_state(0x182C80, (u32)self);", 1)
            old = """    if (func_001526D0(sub, self) != 0) {
        return;
    }
    func_00152470(sub, self);
    if (func_00182FC8(self) != 0) {
        self->desc->m04(self);
    }"""
            new = """    s32 already_linked = func_001526D0(sub, self);
    sly1_native_trace_c02_list(0x182C80, (u32)sub, (u32)self,
                               (u32)already_linked);
    if (already_linked != 0) {
        return;
    }
    func_00152470(sub, self);
    s32 fully_linked = func_00182FC8(self);
    sly1_native_trace_c02_list(0x182FC8, (u32)sub, (u32)self,
                               (u32)fully_linked);
    if (fully_linked != 0) {
        self->desc->m04(self);
    }"""
            if adapted.count(old) != 1:
                raise RuntimeError("expected C01 constructor list gate in " + rel.as_posix())
            adapted = adapted.replace(old, new, 1)
        for address in backfill_addresses:
            name = address_names.get(address)
            if not name:
                continue
            definition = re.compile(
                r"(?m)^(?P<header>[^\n;{}]*\b" + re.escape(name) +
                r"\s*\([^\n{}]*\)[^\n{}]*)\n(?=\s*\{)")
            adapted = definition.sub(
                lambda match: match.group("header").replace(
                    name, "decomp_" + name, 1) + "\n", adapted)
        staged.write_text(adapted, encoding="utf-8")
        unit_flags = flags
        if rel.as_posix() == "main/0016C000.cpp":
            # Avoid depending on the host's optional 32-bit libc headers.
            unit_flags = flags + ["-Dfabsf=__builtin_fabsf"]
        cmd = compiler + unit_flags + ["-I", str(src.parent), "-c", str(staged), "-o", str(obj)]
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if proc.returncode:
            failures.append((rel.as_posix(), proc.stderr))
        else:
            warnings += proc.stderr.count("warning:")
            if proc.stderr:
                print(proc.stderr, file=sys.stderr, end="")

    backfill_objects = objects_root / "port_native"
    if backfill_objects.exists():
        shutil.rmtree(backfill_objects)
    backfills = sorted(backfill_root.rglob("*.cpp")) if backfill_root.exists() else []
    backfill_failures = []
    for src in backfills:
        rel = src.relative_to(backfill_root)
        obj = objects_root / "port_native" / rel.with_suffix(".o")
        obj.parent.mkdir(parents=True, exist_ok=True)
        cmd = compiler + flags + [
            "-I", str(port), "-I", str(port / "runtime" / "include"),
        ]
        if src.name == "sly1_native_loader_bridge.cpp":
            # The freestanding launcher has no 32-bit libc. Loader-stage
            # tracing is optional there; leave the bridge's native dispatch
            # active while compiling its trace hook out.
            cmd += ["-DSLY1_HOST32_FREESTANDING"]
        cmd += ["-c", str(src), "-o", str(obj)]
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if proc.returncode:
            backfill_failures.append((src.relative_to(port).as_posix(), proc.stderr))
        else:
            warnings += proc.stderr.count("warning:")
            if proc.stderr:
                print(proc.stderr, file=sys.stderr, end="")

    print(f"Sly host C++ units: {len(sources) - len(failures)}/{len(sources)} compiled")
    print(f"Port-native backfills: {len(backfills) - len(backfill_failures)}/{len(backfills)} compiled; {warnings} compiler warnings")
    if failures or backfill_failures:
        for name, error in failures + backfill_failures:
            print(f"\n{name}:\n{error}", file=sys.stderr)
        return 1
    print(f"staged sources: {staged_root}")
    print(f"object files: {objects_root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

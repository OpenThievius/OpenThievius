#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Link a freestanding Linux/i386 native Sly executable from the host32 objects."""

import argparse
import re
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--object", required=True, type=Path)
    parser.add_argument("--aliases", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--cxx", default="c++")
    parser.add_argument("--ld", default="ld")
    parser.add_argument("--nm", default="nm")
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)

    nm = subprocess.run([args.nm, "--undefined-only", str(args.object)],
                        text=True, capture_output=True, check=True)
    missing = {}
    for line in nm.stdout.splitlines():
        fields = line.split()
        if len(fields) < 2:
            continue
        symbol = fields[-1]
        match = FUNCTION.search(symbol)
        if match:
            missing[symbol] = int(match.group(1), 16)
    asm = [".section .text.missing,\"ax\",@progbits"]
    for symbol, address in sorted(missing.items()):
        asm.extend([f".globl {symbol}", f".type {symbol},@function", f"{symbol}:",
                    f"    pushl $0x{address:08x}", "    call sly1_missing_function", "    ud2"])
    asm_path = args.out / "missing_functions.S"
    asm_path.write_text("\n".join(asm) + "\n", encoding="utf-8")

    common = ["-m32", "-ffreestanding", "-fno-exceptions", "-fno-rtti", "-fno-pic",
              "-fno-pie", "-fno-stack-protector", "-fomit-frame-pointer", "-fno-builtin",
              "-fno-asynchronous-unwind-tables", "-fno-unwind-tables", "-O2"]
    objects = []
    for name, source in (("start.o", args.source / "start.S"),
                         ("clone.o", args.source / "clone.S"),
                         ("main.o", args.source / "main.cpp"),
                         ("builtins.o", args.source / "builtins.cpp"),
                         ("abi_compat.o", args.source / "abi_compat.cpp"),
                         ("missing_functions.o", asm_path)):
        obj = args.out / name
        command = [args.cxx, *common]
        if source.suffix == ".S":
            command += ["-c", str(source), "-o", str(obj)]
        elif source.suffix == ".cpp":
            command += ["-c", str(source), "-o", str(obj)]
        else:
            command += ["-x", "assembler", "-c", str(source), "-o", str(obj)]
        if not run(command):
            return 1
        objects.append(obj)

    executable = args.out / "sly1_host32"
    linker_script = args.out / "native_host32.ld"
    linker_script.write_text(args.aliases.read_text(encoding="utf-8") + """
ENTRY(_start)
PHDRS { text PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); }
SECTIONS {
  . = 0x08048000;
  .text : { *(.text.start) *(.text .text.*) } :text
  . = ALIGN(0x1000);
  /* EE RAM has no read-only pages. Retail code rewrites text strings while
     wrapping them, so host literals must be writable even though they are
     emitted into .rodata by the compiler. Keep them off the final RX page. */
  .rodata : { *(.rodata .rodata.*) } :data
  . = ALIGN(0x1000);
  .data : { *(.data .data.*) } :data
  . = ALIGN(16);
  .sly1_retail_data : { *(.sly1_retail_data) } :data
  .init_array : {
    __init_array_start = .;
    KEEP(*(.init_array .init_array.*))
    __init_array_end = .;
  } :data
  .bss : { *(.bss .bss.*) *(COMMON) } :data
  /DISCARD/ : { *(.comment) *(.eh_frame) *(.note.gnu.property) }
}
""", encoding="utf-8")
    if not run([args.ld, "-m", "elf_i386", "-T", str(linker_script), "--build-id=none",
                "-o", str(executable), *map(str, objects), str(args.object)]):
        return 1
    print(f"linked {executable}; {len(missing)} unresolved game function symbols trap with retail addresses")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

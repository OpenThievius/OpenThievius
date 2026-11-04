# SPDX-License-Identifier: AGPL-3.0-or-later
"""Extract raw Sly 1 US catalog ranges from a user's ELF and disc.

    python3 port/tools/extract_sly1_catalog.py SCUS_971.98 DISC.iso

Writes each catalog range and a TSV manifest below the ignored build tree.
The data remains local; this tool does not commit or distribute disc content.
"""

import argparse
import csv
import struct
import sys
from pathlib import Path

from extract_sly1_vag import Disc

TABLE_ADDRESS = 0x00247AF0
TABLE_ENTRY_SIZE = 0x2C
TABLE_ENTRY_COUNT = 46
MAX_RESOURCE_BYTES = 256 * 1024 * 1024


def read_elf(path):
    """Return ELF bytes and file-backed PT_LOAD mappings for ELF32 little-endian."""
    data = Path(path).expanduser().resolve().read_bytes()
    if len(data) < 52 or data[:4] != b"\x7fELF":
        raise ValueError("input is not an ELF executable")
    if data[4] != 1 or data[5] != 1:
        raise ValueError("expected a 32-bit little-endian ELF executable")

    header = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
    if header[1] != 2 or header[2] != 8:
        raise ValueError("expected a MIPS executable ELF")
    program_offset, program_entry_size, program_count = header[5], header[9], header[10]
    if program_entry_size < 32 or program_offset + program_entry_size * program_count > len(data):
        raise ValueError("ELF program header table is out of bounds")

    segments = []
    for index in range(program_count):
        offset = program_offset + index * program_entry_size
        kind, file_offset, address, _, file_size, _, _, _ = struct.unpack_from(
            "<IIIIIIII", data, offset
        )
        if kind == 1:
            if file_offset + file_size > len(data):
                raise ValueError("ELF load segment is out of bounds")
            segments.append((address, file_offset, file_size))
    return data, segments


def file_offset(segments, address, size):
    for virtual_address, segment_offset, segment_size in segments:
        if virtual_address <= address and address + size <= virtual_address + segment_size:
            return segment_offset + address - virtual_address
    raise ValueError("ELF does not contain the requested file-backed address 0x%08x" % address)


def read_c_string(data, segments, address, max_length=128):
    offset = file_offset(segments, address, 1)
    end = data.find(b"\0", offset, offset + max_length)
    if end < 0:
        raise ValueError("resource label at 0x%08x is not terminated" % address)
    try:
        return data[offset:end].decode("ascii")
    except UnicodeDecodeError as error:
        raise ValueError("resource label at 0x%08x is not ASCII" % address) from error


def catalog_entries(elf_path, disc):
    data, segments = read_elf(elf_path)
    table_size = TABLE_ENTRY_SIZE * TABLE_ENTRY_COUNT
    offset = file_offset(segments, TABLE_ADDRESS, table_size)
    entries = []
    seen_keys = set()

    for index in range(TABLE_ENTRY_COUNT):
        words = struct.unpack_from("<11I", data, offset + index * TABLE_ENTRY_SIZE)
        key = words[8]
        sector = words[0] ^ words[5]
        byte_count = words[1] ^ words[7]
        label = read_c_string(data, segments, words[9])
        sector_count = (byte_count + 2047) // 2048

        if key in seen_keys:
            raise ValueError("duplicate resource key 0x%x in catalog" % key)
        seen_keys.add(key)
        if byte_count == 0 or byte_count > MAX_RESOURCE_BYTES:
            raise ValueError("resource 0x%x has an invalid byte count" % key)
        if sector >= disc.sectors or sector_count > disc.sectors - sector:
            raise ValueError("resource 0x%x extends beyond the disc image" % key)

        entries.append({
            "key": key,
            "label": label,
            "sector": sector,
            "byte_count": byte_count,
            "sector_count": sector_count,
            "flags": words[10],
        })
    return entries


def extract(elf_path, disc_path, output_dir):
    disc = None
    try:
        disc = Disc(disc_path)
        entries = catalog_entries(elf_path, disc)
        output_dir.mkdir(parents=True, exist_ok=True)
        manifest_path = output_dir / "catalog.tsv"
        with manifest_path.open("w", newline="", encoding="utf-8") as manifest_file:
            writer = csv.DictWriter(
                manifest_file,
                fieldnames=("key", "label", "sector", "byte_count", "sector_count", "flags", "file"),
                delimiter="\t",
            )
            writer.writeheader()
            for entry in entries:
                filename = "resource_%04x.bin" % entry["key"]
                payload = disc.read_logical(entry["sector"], 0, entry["byte_count"])
                (output_dir / filename).write_bytes(payload)
                writer.writerow({**entry, "file": filename})
                print("resource 0x%04x: %d bytes" % (
                    entry["key"], entry["byte_count"]
                ))

        print("extracted %d catalog resources" % len(entries))
        print("output: %s" % output_dir)
        print("manifest: %s" % manifest_path)
        return 0
    except (OSError, ValueError) as error:
        print("error: %s" % error, file=sys.stderr)
        return 1
    finally:
        if disc is not None:
            disc.close()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("elf", help="user-owned NTSC-U Sly 1 boot ELF")
    parser.add_argument("disc", help="matching user-owned cooked ISO or raw 2352-byte BIN")
    parser.add_argument("--output", help="output directory (default: ignored build catalog folder)")
    args = parser.parse_args(argv)

    root = Path(__file__).resolve().parents[2]
    output_dir = (Path(args.output).expanduser().resolve() if args.output else
                  root / "build/port/extracted/catalog")
    return extract(args.elf, args.disc, output_dir)


if __name__ == "__main__":
    raise SystemExit(main())

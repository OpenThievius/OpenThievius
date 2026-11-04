# SPDX-License-Identifier: AGPL-3.0-or-later
"""Extract standalone VAGp samples found in Sly 1's raw data area.

    python3 port/tools/extract_sly1_vag.py DISC.iso [--output DIR]

Outputs are written to an ignored build directory by default. This is a
signature-based recovery tool; it does not resolve Sly's still-unknown disc
index, so extracted files have sector-based names rather than game names.
"""

import argparse
import csv
import struct
import sys
from pathlib import Path

SECTOR_SIZE = 2048
RAW_SECTOR_SIZE = 2352
VAG_HEADER_SIZE = 0x30
DEFAULT_START_SECTOR = 12345
SCAN_CHUNK_SECTORS = 4096
MAX_SAMPLE_BYTES = 128 * 1024 * 1024


class Disc:
    def __init__(self, path):
        self.path = Path(path).expanduser().resolve()
        self.size = self.path.stat().st_size
        self.file = self.path.open("rb")
        self.stride, self.payload, self.base = self._layout()
        if (self.size - self.base) % self.stride:
            raise ValueError("disc image size is not aligned to its sector size")
        self.sectors = (self.size - self.base) // self.stride

    def close(self):
        self.file.close()

    def _has_pvd(self, base, payload, stride):
        self.file.seek(base + 16 * stride + payload)
        return self.file.read(7) == b"\x01CD001\x01"

    def _layout(self):
        if self.size % SECTOR_SIZE == 0 and self._has_pvd(0, 0, SECTOR_SIZE):
            return SECTOR_SIZE, 0, 0
        pregap = 150 * RAW_SECTOR_SIZE
        candidates = [0]
        if self.size > pregap and (self.size - pregap) % RAW_SECTOR_SIZE == 0:
            candidates.append(pregap)
        for base in candidates:
            self.file.seek(base + 16 * RAW_SECTOR_SIZE)
            sync = self.file.read(16)
            if len(sync) != 16 or sync[0] != 0 or sync[11] != 0 or sync[1:11] != b"\xff" * 10:
                continue
            mode = self.file.read(1)
            payload = 16 if mode == b"\x01" else 24 if mode == b"\x02" else 0
            if payload and self._has_pvd(base, payload, RAW_SECTOR_SIZE):
                return RAW_SECTOR_SIZE, payload, base
        raise ValueError("no ISO 9660 primary volume descriptor at logical sector 16")

    def read_logical(self, first_sector, byte_offset, byte_count):
        if byte_offset < 0 or byte_count < 0:
            raise ValueError("negative read range")
        absolute = first_sector * SECTOR_SIZE + byte_offset
        if absolute + byte_count > self.sectors * SECTOR_SIZE:
            raise ValueError("read range extends beyond disc image")
        output = bytearray(byte_count)
        done = 0
        while done < byte_count:
            lba, in_sector = divmod(absolute + done, SECTOR_SIZE)
            amount = min(byte_count - done, SECTOR_SIZE - in_sector)
            physical = self.base + lba * self.stride + self.payload + in_sector
            self.file.seek(physical)
            block = self.file.read(amount)
            if len(block) != amount:
                raise OSError("short read from disc image")
            output[done:done + amount] = block
            done += amount
        return output

    def scan_headers(self, first_sector):
        """Yield (LBA, header) for VAGp markers at logical sector boundaries."""
        physical = self.base + first_sector * self.stride
        remaining = self.sectors - first_sector
        while remaining:
            count = min(remaining, SCAN_CHUNK_SECTORS)
            self.file.seek(physical)
            block = self.file.read(count * self.stride)
            if len(block) != count * self.stride:
                raise OSError("short read while scanning disc image")
            for index in range(count):
                start = index * self.stride + self.payload
                header = block[start:start + VAG_HEADER_SIZE]
                if header[:4] == b"VAGp":
                    yield first_sector + index, header
            physical += count * self.stride
            first_sector += count
            remaining -= count


def parse_header(header, total_sectors, lba):
    if len(header) != VAG_HEADER_SIZE:
        return None
    version, reserved, data_size, sample_rate = struct.unpack_from(">IIII", header, 4)
    if version != 0x20 or reserved != 0:
        return None
    if data_size == 0 or data_size > MAX_SAMPLE_BYTES or data_size % 16 != 0:
        return None
    if sample_rate < 4000 or sample_rate > 96000:
        return None
    if lba * SECTOR_SIZE + VAG_HEADER_SIZE + data_size > total_sectors * SECTOR_SIZE:
        return None
    return data_size, sample_rate


def extract(disc, output_dir, first_sector):
    output_dir.mkdir(parents=True, exist_ok=True)
    manifest_path = output_dir / "manifest.tsv"
    previous_end = first_sector
    extracted = 0
    audio_bytes = 0
    with manifest_path.open("w", newline="", encoding="utf-8") as manifest_file:
        writer = csv.writer(manifest_file, delimiter="\t", lineterminator="\n")
        writer.writerow(("sector", "file", "sample_rate", "adpcm_bytes", "vag_bytes"))
        for lba, header in disc.scan_headers(first_sector):
            if lba < previous_end:
                continue
            parsed = parse_header(header, disc.sectors, lba)
            if parsed is None:
                continue
            data_size, sample_rate = parsed
            vag_size = VAG_HEADER_SIZE + data_size
            sector_span = (vag_size + SECTOR_SIZE - 1) // SECTOR_SIZE
            # A real sector-aligned VAG stream ends exactly where its declared
            # payload predicts. This rejects incidental markers inside media.
            end = lba + sector_span
            if end > disc.sectors:
                continue
            name = "vag_%07d.vag" % lba
            destination = output_dir / name
            destination.write_bytes(disc.read_logical(lba, 0, vag_size))
            writer.writerow((lba, name, sample_rate, data_size, vag_size))
            extracted += 1
            audio_bytes += data_size
            previous_end = end
    return extracted, audio_bytes, manifest_path


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("disc", help="user-owned cooked ISO or raw 2352-byte BIN")
    parser.add_argument("--output", help="output folder (default: build/port/extracted/vag)")
    parser.add_argument("--start-sector", type=int, default=DEFAULT_START_SECTOR,
                        help="first logical data sector to scan (default: %(default)s)")
    args = parser.parse_args(argv)
    if args.start_sector < 0:
        parser.error("--start-sector must be nonnegative")
    root = Path(__file__).resolve().parents[2]
    output_dir = Path(args.output).expanduser().resolve() if args.output else root / "build/port/extracted/vag"
    disc = None
    try:
        disc = Disc(args.disc)
        if args.start_sector >= disc.sectors:
            raise ValueError("start sector is outside the disc image")
        count, total_bytes, manifest = extract(disc, output_dir, args.start_sector)
        print("disc sectors: %d" % disc.sectors)
        print("VAG samples extracted: %d" % count)
        print("ADPCM payload bytes: %d" % total_bytes)
        print("output: %s" % output_dir)
        print("manifest: %s" % manifest)
    except (OSError, ValueError) as error:
        print("error: %s" % error, file=sys.stderr)
        return 1
    finally:
        if disc is not None:
            disc.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

# SPDX-License-Identifier: AGPL-3.0-or-later
"""Extract and decompress one Sly 1 raw-disc LZ stream.

    python3 port/tools/extract_sly1_lz.py DISC.iso --sector LBA --bytes SIZE

The decoder follows the retail 8 KiB scratchpad dictionary reader. This tool
does not resolve the disc index; use it with a sector and compressed byte size
already identified from retail data or a native read trace.
"""

import argparse
import json
import sys
from pathlib import Path

from extract_sly1_vag import Disc

DEFAULT_MAX_OUTPUT = 256 * 1024 * 1024


def decode(source, max_output=DEFAULT_MAX_OUTPUT):
    """Decode one fresh stream: LSB-first flag bits and 13-bit dictionary refs."""
    dictionary = bytearray(0x2000)
    output = bytearray()
    write_position = 0
    position = 0

    while position < len(source):
        flags = source[position]
        position += 1
        for bit in range(8):
            if position >= len(source):
                break

            if flags & (1 << bit):
                value = source[position]
                position += 1
                if len(output) >= max_output:
                    raise ValueError("decoded stream exceeds the output limit")
                output.append(value)
                dictionary[write_position] = value
                write_position = (write_position + 1) & 0x1FFF
                continue

            if position + 1 >= len(source):
                raise ValueError("truncated dictionary reference")
            token = (source[position] << 8) | source[position + 1]
            position += 2
            read_position = token & 0x1FFF
            count = (token >> 13) + 2
            if len(output) + count > max_output:
                raise ValueError("decoded stream exceeds the output limit")
            for _ in range(count):
                value = dictionary[read_position]
                read_position = (read_position + 1) & 0x1FFF
                output.append(value)
                dictionary[write_position] = value
                write_position = (write_position + 1) & 0x1FFF

    return bytes(output)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("disc", help="user-owned cooked ISO or raw 2352-byte BIN")
    parser.add_argument("--sector", type=int, required=True,
                        help="logical sector at the start of the compressed stream")
    parser.add_argument("--bytes", type=int, required=True,
                        help="compressed byte length from the retail stream descriptor")
    parser.add_argument("--output", help="decoded file (default: ignored build extraction folder)")
    parser.add_argument("--max-output", type=int, default=DEFAULT_MAX_OUTPUT,
                        help="maximum decoded size in bytes (default: %(default)s)")
    args = parser.parse_args(argv)
    if args.sector < 0 or args.bytes <= 0 or args.max_output <= 0:
        parser.error("sector must be nonnegative; byte lengths and limits must be positive")

    root = Path(__file__).resolve().parents[2]
    destination = (Path(args.output).expanduser().resolve() if args.output else
                   root / ("build/port/extracted/lzss/resource_%07d.bin" % args.sector))
    disc = None
    try:
        disc = Disc(args.disc)
        if args.sector >= disc.sectors:
            raise ValueError("start sector is outside the disc image")
        compressed = disc.read_logical(args.sector, 0, args.bytes)
        decoded = decode(compressed, args.max_output)
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(decoded)
        manifest = destination.with_suffix(destination.suffix + ".json")
        manifest.write_text(json.dumps({
            "sector": args.sector,
            "compressed_bytes": len(compressed),
            "decoded_bytes": len(decoded),
            "decoder": "retail 8 KiB LZ dictionary stream",
        }, indent=2) + "\n", encoding="utf-8")
        print("compressed bytes: %d" % len(compressed))
        print("decoded bytes: %d" % len(decoded))
        print("output: %s" % destination)
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

<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Sly 1 disc assets

Sly's ISO filesystem contains boot files and IOP modules. Most game media is
stored in raw sectors after the filesystem; the retail loader's index and the
level/model formats are still under investigation. See the decompilation's
[disc research](https://github.com/OpenThievius/sly1-decomp/blob/master/docs/DISC.md).

## Recover standalone audio samples

The raw data area contains sector-aligned `VAGp` samples. The public VAG
header has a 48-byte header with big-endian version, data length and sample
rate fields; its ADPCM payload follows the header. The extractor checks these
fields and requires the payload length to be a multiple of 16 bytes, as
expected for PS-ADPCM blocks. See the [VAG format reference](https://psx-spx.consoledev.net/cdromfileformats/).

Run the scanner against a disc image you own:

```sh
python3 port/tools/extract_sly1_vag.py /path/to/Sly1.iso
```

It accepts cooked 2048-byte ISO images and raw 2352-byte BIN images. By
default it scans from logical sector 12345 and writes the VAG files and
`manifest.tsv` to `build/port/extracted/vag/`. Filenames use sector numbers
because this pass does not recover the game's resource names or index.
Outputs stay in the ignored build tree; no disc data or extracted media is
committed.

On the locally supplied NTSC-U image, the scanner found 1,409 VAGp samples
with 215,281,856 bytes of declared ADPCM payload. This is a signature-based
audio recovery path; it does not locate or extract the level/model data needed
to render and play a level.

## First native raw-sector read

With `OPENRAC_TRACE_CD=1` and `OPENRAC_STARTUP_TRACE=1`, a native startup run
against the US ELF and disc read logical sectors 820372–820445 (74 sectors)
into guest address `0x002c3b00`. The last traced retail function was
`func_00137490`, the stream worker. Its guest object has the source sector at
offset `0x48` (`820372`) and byte length at offset `0x40` (`0x24d41`, or
150,849 bytes); the sector count is the rounded-up length. The read is outside
the ISO filesystem and contains no sector-aligned `VAGp` header or common
zlib, gzip, bzip2 or xz stream at its start. This identifies the stream
descriptor fields, but not the resource name, the index entry that populated
them, or the data encoding.

To capture the read and its caller on a local build, set both variables when
launching `sly1_native` with your own extracted ELF and disc image. This runs
the native launcher; it does not use an emulator.

## Decode a discovered LZ stream

Retail function `func_001378A0` decodes this stream with an 8 KiB scratchpad
dictionary. Each control byte supplies up to eight LSB-first tokens: a set bit
is a literal; a clear bit is a two-byte dictionary reference with a 13-bit
source position. Retail initializes the copy count to the top three bits plus
two and copies through zero inclusively, so the total emitted length is the
top three bits plus three. The decoder in `port/tools/extract_sly1_lz.py`
follows that behavior and writes outputs under the ignored build directory.

For the first native read above:

```sh
python3 port/tools/extract_sly1_lz.py /path/to/Sly1.iso \
  --sector 820372 --bytes 150849
```

The first native raw read reaches `func_00162890` with the same reader object
(`0x0064c580`) that initiated the disc read. At parser entry its decoded input
pointer is `0x70001588`, with 2,680 bytes remaining in the current scratchpad
window; the next 12 bytes are zero. The caller has already consumed fields
from the stream, so the decoded file's initial bytes (`f0 f4 05`) are not this
parser's header. Retail's parser begins by consuming three one-byte fields;
the third controls an array of `0x90`-byte records. The relationship between
the stream's resource data and this parser's input still needs to be recovered.

On the supplied NTSC-U disc the corrected decoder expands 150,849 compressed
bytes to 351,548 bytes. The decoded format and the index entry that selected it
remain unknown, so the port cannot catalog or load a level from it yet.

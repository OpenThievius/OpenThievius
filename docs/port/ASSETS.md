<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Sly 1 disc assets

Sly's ISO filesystem contains boot files and IOP modules. Most game media is
stored in raw sectors after the filesystem. The NTSC-U executable contains a
46-entry catalog of named sector ranges, now extracted by the port tool below;
the level/model formats are still under investigation. See the decompilation's
[disc research](https://github.com/OpenThievius/sly1-decomp/blob/master/docs/DISC.md).

## Extract the NTSC-U catalog

The US executable has a 46-entry range table at `D_00247AF0`, with 44-byte
entries. Retail lookup functions scan it by resource key. Each entry contains
an obfuscated sector and byte count, a key, a pointer to a human-readable
label, and flags. The port extractor resolves these records from a user's ELF,
validates every range against the matching disc, and writes the raw spans and
a TSV manifest to the ignored build directory:

```sh
python3 port/tools/extract_sly1_catalog.py \
  /path/to/SCUS_971.98 /path/to/Sly1.iso
```

The default output is `build/port/extracted/catalog/`. Each raw resource file
uses its numeric key as its filename; the manifest carries the label, sector,
byte count, rounded sector count, and flags. This recovers the catalog and raw
resource spans, not the decoded level/model structures or a playable scene.
The current table address is specific to NTSC-U; PAL has not been validated.

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
because this signature-based pass does not use the catalog.
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
zlib, gzip, bzip2 or xz stream at its start. Later table and runtime tracing
tied it to the first catalog record; the initial transfer trace by itself did
not identify that source.

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
window. That window matches decoded offset 5,512, not the file start. Its first
three bytes are zero, so this call's `0x90`-byte record array has count zero.
The caller has already consumed fields from the stream, so the decoded file's
initial bytes (`f0 f4 05`) are not this parser's header.

The direct caller, `func_001289C0`, first reads three 12-byte groups, another
12-byte field, flags and scalar values, then invokes `func_00162890`. Matching
C shows `func_001289C0` is reused by several object constructors, so this is a
nested object-data reader rather than the disc index or a uniquely identified
level header. The concrete object type and its connection to the containing
resource remain unknown.

The decoder is downstream of a separate buffered source reader. Retail
`func_001378A0` requests compressed input through `func_00137820`; its refill
path uses the stream's source callback (`func_001F8DB8`) and moves data through
an 8 KiB window before exposing it to the object readers. The observed raw
sector transfer itself is handled by `func_00137490`. This separates the
compressed-source path from the nested object parser: the `0x90`-byte zero-count
record array at offset 5,512 is not evidence about the disc index format.

The range fields have a concrete setter: `func_00137328(reader, sector,
byte_count)` stores the sector at `+0x48` and byte count at `+0x40`, while
`func_001373B0` forwards a two-word pair to it. `func_001E4EA8` builds a memory
reader over `D_002C3B00`, derives a range pair from the record in
`D_00275AF0`, and passes it to that setter before invoking the reader's data
callback. The polling loop in `func_0018578C` calls this routine while the
global state is active. This establishes a deferred range-read path, but not
the record's meaning or its relationship to an asset catalog.

`func_001E4EA8` reads eight words through the record pointer stored at
`D_00275AF0 + 4`. It forms the range passed to the reader as
`sector = word[0] ^ word[5]` and
`byte_count = word[1] ^ word[7]`; it rejects a zero byte count. Thus the sector
and size in the live stream descriptor are decoded from a record, not stored
there as plain adjacent values. The remaining words participate in record
validation, but the producer that supplies this block and its place in the
catalog remain unidentified at this point in the loader trace.

The first record's provenance is now established. The static record at
`D_00247AB0` is submitted by `func_00160918` through `func_001F0428` and the
message/state dispatcher. A 10 ms native trace captured `D_00275AF0` active
with its record pointer set to `D_00247AB0`; decoding that record produces
sector 820372 and byte count 150849. The same run's native CD trace then read
sector 820372 for 74 sectors, matching the descriptor's byte count. This
confirms that the deferred range path supplies the first observed raw read.
`func_001EFFA0` is a generic message/state path whose other matching callers
also submit text and screen-position messages, so those callers do not identify
this record's role on their own. The catalog table separately confirms it as
the entry with key `0x2`; resource labels are stored as pointers in the ELF.

Before the nested-record parser, `func_001289C0` calls `func_0011EC40`. That
routine reads signed 16-bit property IDs until a negative sentinel and dispatches
each nonnegative ID through 32-byte entries at `D_002505D8`. A native trace of
the captured reader saw IDs 130 and 145, then the `0xffff` sentinel immediately
before the parser input at decoded offset 5,512. The startup initializer
`func_00109488` populates this dispatch table. In the live entries, ID 130 uses
`func_001BB000`, which calls `func_001B9AE0(self, 3, 0, 3, 0)`; ID 145 uses
`func_001BB138`, which writes a four-bit value into bits 32–35 of the object's
64-bit field at offset `0x538`. The property behaviors are now traceable, but
their asset-level names and the containing resource type are still unknown.

At this parser call, the live object pointer is `0x008476b0` and its runtime
descriptor pointer is `D_00220E20`. That descriptor has value 4, derives from
`D_00217568`, and declares a `0x550`-byte object. Its method table includes
`func_001B7A98` and `func_001B7B78`, which register and unregister the object
with its owner. This identifies the parsed instance as a managed game-object
class; the retail data and matching C do not yet give that class a meaningful
name or tie this resource to a specific level.

On the supplied NTSC-U disc the LZ decoder produces 430,858 bytes from this
150,849-byte compressed span. A capture of the retail decoder's first 8 KiB
matches the corrected extractor byte-for-byte. The parser's captured 2,680-
byte reader window also matches exactly at decoded offset 5,512. This validates
token byte order, dictionary behavior and the observed output window; it does
not yet verify the entire decoded file against retail output. The catalog is
now extractable, but the level/model formats and how the resources compose into
a scene are still unknown, so the port cannot yet load a level.

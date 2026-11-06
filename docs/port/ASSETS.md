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

This is a one-time preparation command. The default output is
`build/port/extracted/catalog/`; keep that directory for later launches. Each raw resource file
uses its numeric key as its filename; a matching `.sectors.bin` file includes
the full sector-rounded range for native CD reads. The manifest carries the
label, sector, byte count, rounded sector count, flags, and both filenames. The
native launcher reads requests covered by those extracted sector files,
including subranges, from this folder (or from `OPENRAC_SLY1_CATALOG_DIR` if
you store the files elsewhere), with ISO reads as a fallback for ranges that
are not in the catalog. The launcher does not run either extractor. Add `--decode-lz` to also run each
range through the retail 8 KiB LZ decoder; decoded files go under the
`decoded/` subdirectory and the manifest gains their decoded sizes. This
recovers the catalog and decoded resource streams, not yet their level/model
structures or a playable scene. The current table address is specific to
NTSC-U; PAL has not been validated.

## Recover standalone audio samples

The raw data area contains sector-aligned `VAGp` samples. The public VAG
header has a 48-byte header with big-endian version, data length and sample
rate fields; its ADPCM payload follows the header. The extractor checks these
fields and requires the payload length to be a multiple of 16 bytes, as
expected for PS-ADPCM blocks. See the [VAG format reference](https://psx-spx.consoledev.net/cdromfileformats/).

This optional research scan is also a one-time command; its output is not
needed to launch the game. Keep the resulting VAG files if you want them for
later audio work. Run the scanner against a disc image you own:

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

Retail `func_001E4EA8` confirms that the pending request is handled as a
deferred engine job, not as a direct synchronous disc read. It first clears an
8-byte temporary record, decodes the request's 32-byte record, and clears the
request's active pointer. It then initializes a memory-backed reader over
`D_002C3B00` (size `0x340C00`, alignment `0x40`, six units), attaches a
`0x60`-byte stream descriptor, and passes the decoded sector/byte-count pair
to `func_001373B0`. On success, retail starts the stream object's callback at
vtable offset `0x38`, invokes the reader's finalize method, and registers the
job with the engine. It also configures the active resource ID, progress UI,
and loader state before returning. The native C backfills currently establish
the request and range fields, but do not yet reproduce this coordinator or
worker lifecycle. Retail `func_00137490` owns the asynchronous sector ring and
refill/read-window behavior. The port's current coordinator instead reads the
whole descriptor synchronously into `D_002C3B00`; the worker stub is reached
only after those 150,849 bytes have been consumed. It is therefore not the
cause of the current EOF inside the second collider.

This makes `func_001E4EA8` a useful implementation target, but not a safe
place to report a resource as complete. Its callback, job registration, and
reader-finalization effects must either run through native Sly code or have
behavioral port replacements before the Splash request can advance. The
current `sly1-decomp` checkout has matching C for `func_00137490` (but not for
`func_001E4EA8`). Its ring-buffer implementation is a useful reference if the
port returns to asynchronous sector reads. For the present synchronous path,
the unresolved issue is why the parser requests a 65,535-face block after the
descriptor's bytes are exhausted; verify the serialized collider boundary and
the decoded-length/header semantics before adding more reader behavior.

The current trace narrows the first suspicious cursor transition. The top-level
object list begins with count 36 and a plausible first record (type 4, key 1,
flag `0xFFFF`). Its collider reader returns at decoded offset `0x1594`; the
next record then reads type `0x5300`, which is outside the retail descriptor
table. A later type-5 record enters the collider reader and yields 6,626
vertices, followed at decoded offset `0x14CE1` by face count `0xFFFF`. The
decoded bytes match an independent windowed translation of retail's LZ logic,
so these values are present in the decoded stream. The list transition after
the first collider is now the earliest concrete alignment lead; establish
whether the object payload or a preceding world section is being consumed
incorrectly before treating the later count as a retail asset sentinel.

The retail callback chain narrows the missing work further. After setting the
range, `func_001E4EA8` requests engine object type `0x6C` from
`func_0011E580`, then invokes the returned object's method at vtable offset
`+0x38` with the stream reader. For the NTSC-U type table, that method is
`func_001DCEA0`; its retail implementation parses the loaded resource and
constructs additional objects, including types `0x6D`, `0x1E`, and `0x4D`.
The coordinator then finalizes the stream reader and registers the job. The
type `0x6C` constructor is `func_001DB4C8`; its base constructor is
`func_001DB480`. Port-only backfills for the constructor and its pool/vector
helpers `func_001DC740` and `func_001DBA90` now compile in the native host
build.

This means a native loader backfill cannot safely replace `func_001E4EA8` with
a raw sector read or mark the request complete after decompression. The
The coordinator (`func_001E4EA8`) has a port-only backfill that reads the
range, constructs a type `0x6C` object, invokes its parser callback, and
finalizes the reader. It does not yet reproduce the full progress/job
bookkeeping. The native CD read fills the reader's backing ring; the compressed
reader now uses the retail source cursor and control-byte sentinel. A bounded
run matches the offline decoder's first 8 KiB. A later bounded run with loader
stage tracing shows the parser reading flag `0`, value `1`, and table count
`1491`, then reaching callback stages 1–5 at decoded offset `0x15af`. The
defensive oversized-count branch in the port backfill is not taken. The prior
`0x6876bb00` diagnosis was a misread of bytes overlapping the compressed
header, not the table count. This Splash parse still does not establish level
loading: Splash is resource key `0x2`, while the level resource is `0x0100`.
Port-only overrides for `func_0018D608`,
`func_0018D658` and `func_0018D6F0` provide a thread-local packet arena in
reserved guest-port memory. A bounded native run read sector 820372 for 74
sectors, avoided the previous invalid allocation fault, and submitted 984
accepted GIF chains over roughly 12 seconds. The display framebuffer remains
unset and no scene is verified as loaded. Hostgen emits no-C stubs for several
reader helpers because it cannot parse all of `00134000.cpp`; the missing C++
member-function support and a conflicting inferred declaration are documented
in the build report. The buffered reader is compiled but its complete refill
path is not verified. A port-only `func_0011E580` backfill implements retail
descriptor lookup, allocation, owner-list registration, and constructor
dispatch through the native function map. The port-only `func_00137A60`
backfill reproduces the buffered byte-reader loop that requests data from the
stream worker; `func_0018D800` supplies its retail byte-copy operation. These
Sly-specific behaviors
live under `port/game/sly1-us/hand/native_c/`, separate from matching
decompilation. The parser, reader refill and job lifecycle still need a
working native implementation before this resource can be considered loaded.

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

A static direct-call scan finds `func_001E4EA8` called from `func_00185758`;
`func_00160918` is the separate startup submission of record `0x2` (Splash).
Therefore the observed Splash read and nested-object parse do not establish how
level resource `0x0100` uses its leading in-resource offset. That level header
still needs to be connected to its own loader and reader calls.

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

Static disassembly of retail `func_00162890` (4,264 bytes; still not matched
to C) confirms that it is a nested object-data parser, not a top-level level
header. It reads small scalar fields through `func_00137B90`, allocates arrays
using counts from the stream, copies fixed-size blocks, and handles flags that
select additional per-object fields. These observations support the live trace
but do not yet define a safe standalone record schema; the callers and stream
position still determine the meaning of those fields. No decoder for this
function's records should be treated as a level loader until the caller chain
and containing resource are established.

On the supplied NTSC-U disc the LZ decoder produces 430,858 bytes from this
150,849-byte compressed span. A capture of the retail decoder's first 8 KiB
matches the corrected extractor byte-for-byte. The parser's captured
2,680-byte reader window also matches exactly at decoded offset 5,512. A later bounded runtime capture compared all 430,858 decoded
bytes against the extracted Splash stream with no mismatch. This validates the
full output for the observed Splash request, in addition to token byte order
and dictionary behavior. The integrated catalog extraction run successfully decoded all 46 NTSC-U ranges, producing
285,281,013 bytes total (430,858–8,669,578 bytes per resource). That confirms
the streams are accepted by the observed decoder, but not that the full outputs
match a retail capture. The level/model formats and how the resources compose
into a scene are still unknown, so the port cannot yet load a level.

## Native C++ resource-loading prototype

`port/sly1/resource_loader.cpp` is a standalone host-native C++ implementation
of the independently established NTSC-U catalog and raw-disc LZ path. It reads
catalog records from the user's ELF, reads corresponding raw logical sectors
from a cooked ISO or raw BIN image, and applies the retail 8 KiB dictionary
decoder. It is separate from the experimental guest execution runtime and does
not interpret the decoded resource as a level.

Build and inspect catalog key `0x0100` (“A Stealthy Approach”) with:

```sh
cmake -S port -B build/port/native-resource
cmake --build build/port/native-resource --target sly1_resource_inspect
build/port/native-resource/sly1_resource_inspect \
  /path/to/SCUS_971.98 /path/to/Sly1.iso 0100 /tmp/level-resource.bin
```

All 46 catalog outputs from the C++ prototype were compared byte-for-byte with
the existing Python extractor output. Key `0x0100` (“A Stealthy Approach”)
decodes to 7,352,657 bytes, with identical SHA-256 in both implementations.
This verifies catalog lookup, exact-length disc reads and decompression across
the NTSC-U catalog; the decoded scene/container format and renderer inputs
remain to be identified.

## Container-format lead

Each of the 46 decoded catalog resources begins with a 32-bit little-endian
value that is 16-byte aligned and falls within that resource's decoded byte
range. For key `0x0100` the value is `0x00240cd0`; bytes at that offset begin
with small little-endian fields followed by IEEE-754 float-looking values.
This strongly suggests an in-resource offset or pointer, but its target's
meaning and the rest of the container layout are not established. Treat it as
a format lead, not a verified scene root. The next step is to trace the retail
loader's use of this header field and correlate it with the resource-reader
calls before assigning names or parsing it as geometry.

A 2026-11-06 follow-up traced the Splash reader at each pre-list boundary:
`func_00138550` consumes exactly `0x20` bytes per call, `func_001384A0`
consumes a little-endian 16-bit length plus that many bytes, and
`func_001B3040` consumes the animation/image tables before the object list.
These port backfills match the current decomp C's stated field widths and
sequence. In the observed run their cursor positions are `0x49` before the
first optional block, `0x69` after it, `0x6B` after the buffered string,
`0x8B` after the second optional block, and `0x14D5` after the animation
tables. The first object list count and row then decode at `0x14D5` as 36 and
(type 4, key 1, flag `0xFFFF`). This rules out a simple cursor-width error in
those generic readers, but does not validate the opaque table payloads or the
world/property state that precedes them.

The next comparison target is `func_001C1190` and `func_0011EC40`, which parse
world script values and property records before the cursor stage at `0x49`.
The port has separate implementations in `resource_value_parser.c`; compare
each property reader against its retail class-specific path and inspect the
bytes at each cursor boundary. The exact current decomp reference is
`src/main/file_1C05D0.cpp` and `src/main/0011C000.cpp` in the adjacent
`sly1-decomp` checkout. Keep fixes to those missing or replaced behaviors in
this port repository until the matching decompilation owns them.

The 2026-11-06 follow-up checked the pre-list cursor readers against the
current decomp checkout at `cf9ad464696a197803f584980bff43ed62b6bf31`.
`func_0011BA28`, `func_0011E680`, `func_0011ECB8`, the collider field readers,
and `func_00128C10` agree on the observed serialized widths. In the captured
first collider, `func_001289C0` starts at `0x1538`; its preceding transform,
property, and nested-object fields reach `func_00162890` at `0x1587`. That
reader consumes six bytes and returns at `0x158D` with record count zero.
`func_00128C10` then consumes its four-byte zero flags,
then the one-byte group count and two-byte empty child list end at `0x1594`.
The next list row is the raw bytes `00 53 00 4B 05 FF` (type/slot `0x5300`,
key `0x4B00`, flag `0xFF05`), so this is not a trace-formatting problem.
`func_00162890` remains assembly-only in decomp, and its port backfill is the
main unverified reader on this path; compare its complete disassembly with the
backfill before changing record boundaries. The pre-list readers' matching
widths narrow the lead but do not prove the parsed world data or object-list
structure is correct.

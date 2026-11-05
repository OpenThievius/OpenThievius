<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Native port backfills

This directory contains functional replacements for the host-native C++ build
only. These files are separate from `sly1-decomp/src`, are not matching
sources, and never affect decompilation progress. The native host32 build
compiles them into `build/port/host32/objects/port_native/` and maps their
function names to retail addresses alongside compiled decomp functions. The
current upstream snapshot is `a0fd295` (PR #54) from
[`OpenThievius/sly1-decomp`](https://github.com/OpenThievius/sly1-decomp); the
separate working decomp checkout remains untouched. The host32 build compiles
84 selected Sly C++ units, including recent matching functions `func_001A0BA0`,
`func_00180398`, and `func_001F5CD0`. PR #54 adds readable button-code names
and notes, but no new US function matches or function bodies relevant to
startup. Against this snapshot, 84/84 Sly units and 82/82 port-native C++
backfill objects compile. The combined object provides 3,852 of 4,806
catalogued function addresses; its direct-call frontier resolves 699 of 752
functions. The host32 linker lays address-named C globals out at their retail
relative offsets, preserving adjacent objects used as larger structures, and
keeps that range writable and aligned in the native ELF. The native startup
bridges complete the two sound SIF bootstrap calls, suppress sound bank RPCs,
and report the already-open disc as ready. Host staging pads the three
undersized T217D backing arrays so native class initialization does not
overwrite neighboring vtables. Port-only DMA channel lookups and waits carry
startup beyond both DMA callbacks.

The native GIF-DMA boundary now preserves the retail QWC size and submits raw
GIF packets separately from DMAC tag chains. Native host-pointer spans are
checked against mapped pages before the renderer reads them. A five-second
SDL run on 2026-11-05 opened a window and accepted two 16-byte display packets,
but remained black. The newer freestanding host32 diagnostic maps the US ELF
and ISO, parses the common GLOBSET records, and enters nested scene objects.
Its startup path now passes the position transforms and stops at the next
unresolved retail call. The port is not playable yet. These runtime services
and functional C backfills remain separate from matching C; the matching
`func_001F8290` implementation remains untouched in the decomp's
`nonmatching/` directory.
Matching definitions discovered upstream replace the corresponding old port
backfills.

Run it with the extracted NTSC-U ELF and a cooked ISO or raw BIN:

```sh
build/port/host32/launcher/sly1_host32 <SCUS_971.98> <Sly 1 ISO or BIN>
```

Remove a backfill as soon as matching C/C++ for that address is available;
the combine step rejects an address defined by both trees. Keep these
replacements behavior-focused and derive their state changes from Sly 1's own
retail executable.

`linux_semaphores.cpp` supplies Linux i386 host services for Sly's retail
`CreateSema`, `DeleteSema`, `SignalSema`, `iSignalSema`, `WaitSema`, `PollSema`
and `iPollSema` wrappers. It uses host futexes and does not run PS2 instructions.
The semaphore boundary can support native host threads, but the freestanding
host32 diagnostic path still lacks a graphics setup service and a visible
presentation path.

`sly1_native_cd.cpp` replaces the retail sector-read call at the native
boundary. The launcher opens the user's ISO/BIN with Linux large
file support, detects cooked 2048-byte or raw 2352-byte sectors, and reads
logical sector payloads directly into the mapped Sly address space. Reads are
synchronous, so the wait service reports no outstanding request. This makes
the CD boundary available to native loader code; it does not implement Sly's
stream refill worker or prove that a level has loaded.

`func_001430F8.cpp` replaces the reachable CP0 Count read with a monotonic host
counter scaled to the NTSC-U 294.912 MHz tick rate used by Sly's timer code.
It is a port-only platform service; the matching source remains unchanged.

`func_00128018.cpp` replaces the common affine coordinate helper with native
float operations. It preserves the retail copy cases and computes the three
local-space coordinates from the transform basis and translation. The retail
routine also leaves intermediate values in VU0 registers; the native backfill
does not model those registers, and carries the input homogeneous lane through
the output. This is port-only math for host-native callers, not matching source.

`func_001EACA0.cpp` and `func_001BC260.cpp` implement the quadratic-root and
coordinate-normalization steps used by Sly's movement update. They use native
single-precision math and do not depend on the guest VU state.

`func_001F6C20.cpp` preserves Sly's DMA reset call as a native no-op because
the port completes DMA submissions synchronously and has no pending hardware
queue to reset. The previous-frame frontier showed this wrapper as a missing
direct call; the host32 map now resolves it.

`func_001388A0.cpp` removes a C03 body's contact-index entries from the three
fixed-size banks. It is a port-only implementation based on the matching
`func_00138828` layout and remains separate so the decompilation can replace it.
The current decomp checkout has an 88.7% near miss for this address in
`nonmatching/func_001388A0.cpp`; it is not yet a matching definition.

`func_00185D20.cpp` and `func_001860A8.cpp` prepare and dispatch Sly's
0x100-byte object draw entries without executing VU instructions.
`func_00185D20` uses the existing native affine transform backfill;
`func_001278A8.cpp` and `func_001280C0.cpp` provide the object transform walk
and relative-vector math with host scalar operations. The indirect callbacks
still need an adapter to the reused OpenRAC renderer or a Sly world-draw path.

`func_001B5F50.cpp` parses scene reference and entry records, consumes property
values without running their callbacks, and constructs nested child objects
through the matching factory. Its scene data shape follows the retail EE
assembly; missing property callbacks remain port work.

`func_0011FC90.cpp` consumes position-track data through the matching reader
and currently defaults its result to false. `func_0011F6B0.cpp` initializes
optional track output buffers but does not evaluate the track curves. These
are explicit startup fallbacks; they need full math translations or matching
decompilation before position-driven gameplay can work.

`func_00189150.cpp` translates the three-vector transform previously executed
by the EE VU0 macro into scalar host math. The host staging pass also adjusts
the `func_001EAD88` definition to the angle-first calling convention used by
its native callers; neither change modifies the decompilation checkout.

`func_00188D28.cpp` supplies the next matrix-to-axis-angle step using
scalar host math. Both transform functions remain port-only backfills until
matching decompilation code replaces them.

The scene-object path also has port-only replacements for the EE transform
helpers `func_00128150` and `func_00188520`, object initialization and parent
updates (`func_00124868`, `func_001245A8`), attachment transforms
(`func_00128FD0`), and the vector thresholds `func_00123DC0` and
`func_00123E00`. The host32 trace now advances into mesh rendering at
`func_001662A8`, which remains unresolved; the native port is still not
playable.

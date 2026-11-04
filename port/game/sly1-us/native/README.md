<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Native port backfills

This directory contains functional replacements for the host-native C++ build
only. These files are separate from `sly1-decomp/src`, are not matching
sources, and never affect decompilation progress. The native host32 build
compiles them into `build/port/host32/objects/port_native/` and maps their
function names to retail addresses alongside compiled decomp functions.

Remove a backfill as soon as matching C/C++ for that address is available;
the combine step rejects an address defined by both trees. Keep these
replacements behavior-focused and derive their state changes from Sly 1's own
retail executable.

`linux_semaphores.cpp` supplies Linux i386 host services for Sly's retail
`CreateSema`, `DeleteSema`, `SignalSema`, `iSignalSema`, `WaitSema`, `PollSema`
and `iPollSema` wrappers. It uses host futexes and does not run PS2 instructions.
The semaphore boundary can support native host threads, but the host32 build
still lacks its thread creation/entry service and a linked game launcher.

`func_001430F8.cpp` replaces the reachable CP0 Count read with a monotonic host
counter scaled to the NTSC-U 294.912 MHz tick rate used by Sly's timer code.
It is a port-only platform service; the matching source remains unchanged.

`func_001FB6B0.cpp` supplies the signed 64-bit tick-to-float conversion used
by Sly's frame timers. It uses the host's IEEE-754 conversion and remains a
port-only implementation until matching C is available.

The native backfills for `func_0016E118`, `func_001DDA20` and
`func_001DDA50` preserve the retail flag and reference-count transitions used
by the frame-state path. Their retail instruction sequences and matching
call-site layouts were inspected before writing the replacements.

`func_001FAE18.cpp` runs the NTSC-U initializer table in reverse order through
the generated native address map. The five interior callback wrappers preserve
the retail `(1, -1)` arguments; `func_00211DE0.cpp` preserves its two global
addresses. `func_001AEA70.cpp` and `func_001F2688.cpp` initialize the fixed Sly
configuration records those callbacks target. All 18 initializer-table entries
now resolve to native function definitions in the host32 combine output.

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

`func_00185D20.cpp` and `func_001860A8.cpp` prepare and dispatch Sly's
0x100-byte object draw entries without executing VU instructions.
`func_00185D20` uses the existing native affine transform backfill;
`func_001278A8` still needs a native implementation, and the callbacks need an
adapter to the reused OpenRAC renderer or a Sly world-draw path.

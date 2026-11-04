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

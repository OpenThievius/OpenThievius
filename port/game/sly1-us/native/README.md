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

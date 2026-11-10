<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Superseded port backfills

These files preserve earlier port-only implementations that no longer own
their retail addresses. They are not compiled by the direct host32 build.
Where a legacy hostgen mapping still exists in `backfills.tsv`, it is retained
only for the older configured decomp checkout and should be retired when that
build switches to the current source.

`draw_entries.c`, `func_00185D20.cpp`, and `func_001860A8.cpp` treated
`0x00185D20` and `0x001860A8` as draw-entry functions. The current decomp maps
them to `CreatePooledContact` and contact-chain processing. Let the matching
decompilation own both addresses; do not restore these backfills.

`func_0011E580.cpp` was a host-only object-factory fallback. The latest decomp
now provides `CreateTypedRecord` at that address, so the direct host32 build
uses the decomp definition. The legacy hostgen override remains only while its
configured source checkout lacks the matching implementation.

The latest decomp now has matching C for the moved loader, pose-curve, object
transform, collision, camera, text, and state-graph functions. Those files
previously replaced the matching source, including temporary no-op boundaries
and incomplete curve fallbacks. Keep them here as reference for the earlier
port behavior; let the decomp implementations own their retail addresses.

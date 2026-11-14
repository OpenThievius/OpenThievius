<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Superseded port backfills

These files preserve earlier port-only implementations that no longer own
their retail addresses. They are not compiled by the direct host32 build.
Legacy hostgen mappings are removed once the current decomp supplies matching
C. The files remain here only as history and evidence for the earlier port
backfills; they are not compiled.

`draw_entries.c`, `func_00185D20.cpp`, and `func_001860A8.cpp` treated
`0x00185D20` and `0x001860A8` as draw-entry functions. The current decomp maps
them to `CreatePooledContact` and contact-chain processing. Let the matching
decompilation own both addresses; do not restore these backfills.

`func_0011E580.cpp` was an earlier host-only object-factory fallback;
`func_0011E580.c` was the later hostgen form. The decomp provides
`CreateTypedRecord` at that address, so the direct host32 build uses the decomp
definition. Its legacy hostgen override is retired too.

`func_00162890.cpp` was an incomplete GLOBSET model reader. It skipped the
variable strip payloads and left the serialized mesh index (for example
`0x22`) in the field that the renderer reads as a `StripMesh*`. The wave 15
decomp now supplies the complete matched `ReadPlacedObjectModel`; the direct
host32 build uses that implementation, which gets past attachment DMA setup.
The archive copy is reference only. The direct build no longer compiles it.

The latest decomp now has matching C for the moved loader, pose-curve, object
transform, collision, camera, text, and state-graph functions. Those files
previously replaced the matching source, including temporary no-op boundaries
and incomplete curve fallbacks. Keep them here as reference for the earlier
port behavior; let the decomp implementations own their retail addresses.

On 2026-11-14, hostgen confirmed these ten addresses have matched C in the
current US source and no longer need port overrides: `func_0011A338`
(`AddScriptScopeParent`), `func_0011E580` (`CreateTypedRecord`), `func_00154EE0`
(`ReadCappedWordList`), `func_0015C1C0` (`GetFontById`), `func_0018D608`,
`func_0018D658`, `func_0018D6F0` (back-memory arena), `func_001B4A10`
(`ReadAnimator`), `func_001B89B8` (`RecomputeColliderFrame`), and
`func_001B93D8` (`DrawCollider`). Their previous port bodies are archived here;
the hostgen override table no longer shadows the matching source.

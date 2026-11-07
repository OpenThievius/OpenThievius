<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Sly 1 port restart checkpoint

Updated: 2026-11-07, after commit `a115f82`. Work in
`/home/reaper/sly/OpenThievius` on branch
`wip/native-sly-port`. The user wants frequent commits and pushes. Keep the
port-only functional C backfills under `port/game/sly1-us/hand/native_c/` and
registered in `backfills.tsv`; do not put those replacements in
`/home/reaper/Code/sly1-decomp`. Continue using native runs only; no emulator
is involved in the port work.

## State

- Latest pushed commit at this update: `a115f82 docs(sly1): preserve resource watchpoint findings`.
- The checkout was clean and matched `origin/wip/native-sly-port` when this
  update was written.
- The local decomp checkout `/home/reaper/Code/sly1-decomp` was fetched and
  checked: clean `master` at `e1b39f1 feat(us): match small function batch`;
  the routines relevant to the current object/render investigation were still
  in progress there.
- Build succeeded with 2,043 translated C functions, 222 assembly
  translations, and zero stubs. Rebuild generated code with
  `cmake --build build/port --target port_native -j2` after hostgen changes.

## Actual graphics blocker

The native window starts, but the current level probe produces no scene
vertices or visible text. The direct `OPENRAC_BOOT_LEVEL=0x0100` probe reaches
loader stage 5 but does not connect that loaded world to gameplay. Five outer
groups (`0x00fe903c`, `0x00fe942c`, `0x00fe981c`, `0x00fe9c0c`,
`0x00fe9ffc`) remain skipped by their resource flag; preserve that retail
behavior. A separate render list has an unskipped entry at `0x00847ce0` with
three primary records. Their descriptor at `0x0083f4b0` points to three
nonzero 32-byte blocks at `0x0064d6e0`, `0x0064d8e0`, and `0x0064dae0`, but
the retail packet builder outputs only command tags and zero DMA addresses.
The exact pointer chain and sample bytes are now written in
`docs/port/SLY_GRAPHICS_HANDOFF.md`, so `/tmp` captures are not required to
resume. Exact US assembly is also enabled for `func_001DC9A0`, the 800-byte
level effect initializer that had been an empty no-C stub. The latest bounded
run remains frame state 3/game mode 0 with zero scene vertices.

### Latest producer-side evidence (2026-11-07)

A native GDB hardware watchpoint was armed on guest EE address `0x0064d6e0`
before direct-level startup (`OPENRAC_BOOT_LEVEL=0x0100`). It fired during
`func_001DBA38`'s reader copy, through `func_001DCEA0` and
`func_001DBA38 -> func_00137A60 -> func_0018D800`. At the copy routine,
registers were `rdi=0x0064cae0`, `rsi=0x0c00`, `rdx=host-mapped guest buffer`,
`rcx=0x0064d6e0`; the destination is exactly `0x0c00` bytes after the copy
start. This ties the watched block to the direct-level resource reader, but
does not yet identify the block's format or the render-list consumer. The
retail C for `func_001DBA38` is in
`/home/reaper/Code/sly1-decomp/src/main/001D8000.cpp` around line 206: it reads
`D_00275720` bytes into `D_00275718` after advancing the reader by `0x10`.
All three 32-byte samples were confirmed in the decoded resources: level
`0x0100` at `0x4d0470`, `0x4d0670`, `0x4d0870`, and Splash `0x0002` at
`0xac00`, `0xae00`, `0xb000`. Each sequence has a `0x200` stride. This proves
the payload repeats across the two resources but does not identify its format
or show that it is mesh data.

A follow-up GDB read watchpoint first caught the host renderer copying a
`0x200`-byte DMA payload starting at guest `0x0064d6e0`. Its stack was
`func_001B3450 -> func_001692B0 -> func_001686E8 -> func_00202A80` and then
the native `openrac_native_renderer_submit_dma_chain` adapter. This is the
renderer gathering a DMA payload, not proof that `func_001662A8` consumes the
bytes as geometry. Correlate the referencing DMA tag and retail caller before
assigning a format.

The write-watch GDB script was `/tmp/sly-watch-mesh-first-write.gdb`; the read
watch script and output are `/tmp/sly-watch-mesh-read.gdb` and
`/tmp/sly-watch-mesh-read.log`. Both derive guest base from native symbol
address `0x724e90`. The source-level reader and current packet evidence are now
preserved here and in `docs/port/SLY_GRAPHICS_HANDOFF.md`.

Useful captures, if `/tmp` survives restart:
`/tmp/sly-native-direct0100-mesh-sections.log`,
`/tmp/sly-native-direct0100-packet-dump.log`,
`/tmp/sly-native-direct0100-dc9a0-asm.log`,
`/tmp/sly-native-direct0100-render-groups-final.log`, and
`/tmp/sly-watch-entry-flag-detail.log`. Read
`docs/port/SLY_GRAPHICS_HANDOFF.md` and
`docs/port/PLAYABLE_TODO.md` before repeating older probes; they contain prior
startup, rendering, and decomp coverage findings.

## Resume here

Trace the consumer of the three packed blocks above and establish
why the unskipped render-list entry has zero DMA addresses. Do not force the
skipped groups or synthesize packet data. Separately trace the normal
Splash-to-title/New Game transition; normal startup still stays on Splash
(frame state 3/game mode 0) and does not reach the opening sequence or title.
Keep the direct-level probe diagnostic-only. Keep all functional C replacements
as clearly labeled port-only backfills, separate from matching decompilation.

Native launcher command used for the direct-level probe:

```sh
OPENRAC_BOOT_LEVEL=0x0100 build/port/sly1_native \
  build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

The native window opening successfully is not a playable or visible milestone.

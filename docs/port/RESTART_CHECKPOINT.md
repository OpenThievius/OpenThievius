<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Sly 1 port restart checkpoint

Updated: 2026-11-07. Work in `/home/reaper/sly/OpenThievius` on branch
`wip/native-sly-port`. The user wants frequent commits and pushes. Keep the
port-only functional C backfills under `port/game/sly1-us/hand/native_c/` and
registered in `backfills.tsv`; do not put those replacements in
`/home/reaper/Code/sly1-decomp`. Continue using native runs only; no emulator
is involved in the port work.

## State

- Latest pushed commit: `f492647 debug(sly1): capture every level entry skip word`.
- The checkout was clean and matched `origin/wip/native-sly-port` when this
  checkpoint was written.
- The local decomp checkout `/home/reaper/Code/sly1-decomp` was fetched and
  checked: clean `master` at `e1b39f1 feat(us): match small function batch`;
  the routines relevant to the current object/render investigation were still
  in progress there.
- Build previously succeeded with 2,043 translated C functions, 221 assembly
  translations, and zero stubs. Rebuild generated code with
  `cmake --build build/port --target port_native -j2` after hostgen changes.

## Actual graphics blocker

The native window starts, but the current level probe produces no scene
vertices or visible text. The direct `OPENRAC_BOOT_LEVEL=0x0100` probe reaches
loader stage 5 but does not connect that loaded world to gameplay. The five
outer render groups are `0x00fe903c`, `0x00fe942c`, `0x00fe981c`,
`0x00fe9c0c`, and `0x00fe9ffc`; their primary-record counts are 1, 1, 1, 1,
and 6. The first entry in each has `+0x28 == 1`, and retail `func_001662A8`
returns early for that value, so all five groups submit zero packets. A GDB
watchpoint established that retail `func_00162890` writes this byte from the
resource stream. Preserve this retail behavior; do not clear the flag or force
packet generation. These records have not yet been shown to be renderable
objects.

Useful captures: `/tmp/sly-native-direct0100-render-groups-final.log` and
`/tmp/sly-watch-entry-flag-detail.log`. Read
`docs/port/SLY_GRAPHICS_HANDOFF.md` and
`docs/port/PLAYABLE_TODO.md` before repeating older probes; they contain prior
startup, rendering, and decomp coverage findings.

## Resume here

Trace the direct-level resource record path and determine which serialized
object kinds create mesh-bearing/renderable entries. The five groups above
are likely not geometry packets given their retail skip field. Alternatively,
trace the normal Splash-to-title/New Game transition; normal startup still
stays on Splash (frame state 3/game mode 0) and does not reach the opening
sequence or title. Keep the direct-level probe diagnostic-only. Do not make a
graphics workaround that changes retail function behavior without isolating
it as a clearly labeled port backfill.

Native launcher command used for the direct-level probe:

```sh
OPENRAC_BOOT_LEVEL=0x0100 build/port/sly1_native \
  build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

The native window opening successfully is not a playable or visible milestone.

<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Sly 1 port restart checkpoint

Updated: 2026-11-07, after verifying the resource-service path. Work in
`/home/reaper/sly/OpenThievius` on branch
`wip/native-sly-port`. The user wants frequent commits and pushes. Keep the
port-only functional C backfills under `port/game/sly1-us/hand/native_c/` and
registered in `backfills.tsv`; do not put those replacements in
`/home/reaper/Code/sly1-decomp`. Continue using native runs only; no emulator
is involved in the port work.

## State

- The port-only `func_001E4EA8` now mirrors the
  retail per-frame service tail even when no resource request is pending,
  and after completing a resource request. The tail includes camera/world
  updates and `func_001C06D8`; the latter and `func_001C0070` are selected from
  exact US assembly because they have no matching C yet. The C backfill stays
  isolated under `hand/native_c/`.
- Added the missing `func_00160CE8` counter reset as a port-only backfill:
  decomp C has the body, but hostgen still classifies it no-C. It is listed in
  `hand/backfills.tsv` only (not `overrides.tsv`, which is for replacing
  matched definitions).
- Build succeeded with `cmake --build build/port --target port_native -j2`.
  The ordinary US ELF/ISO native run completed its 8-second trace window with
  no `func_00160CE8` warning, but remains at `sly_frame=0,3,0`, game mode 0,
  and `visible_text=0`. This rules out the missing counter reset as the full
  startup fix. Log: `/tmp/sly-native-counter-reset-verified.log`.
- Exact assembly comparison remains: the assembly has a pre-reader
  `func_001430B8` timer call; the current backfill still has an older
  `func_00142ED0(1.0f)` after parsing. Recheck this sequencing against the
  retail body before treating this patch as complete.

- Previous pushed baseline: `67afef0 fix(sly1): restore retail resource service tail`.
- Next: trace the normal Splash-to-title transition/request producer and its
  state selector callbacks. Keep the direct-level probe diagnostic-only.

## Previous state

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
512-byte GS image payloads at `0x0064d6e0`, `0x0064d8e0`, and `0x0064dae0`.
Retail `func_001662A8` still outputs command tags and zero scene geometry
addresses. The exact pointer chain and transfer evidence are written in
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
start. This ties the watched image payload to the direct-level resource
reader; the later DMA trace identifies its format, but not the render-list
consumer. The
retail C for `func_001DBA38` is in
`/home/reaper/Code/sly1-decomp/src/main/001D8000.cpp` around line 206: it reads
`D_00275720` bytes into `D_00275718` after advancing the reader by `0x10`.
All three 512-byte payloads were confirmed in the decoded resources: level
`0x0100` at `0x4d0470`, `0x4d0670`, `0x4d0870`, and Splash `0x0002` at
`0xac00`, `0xae00`, `0xb000`. Each sequence has a `0x200` stride. The image
payloads are shared across the two resources.

Filtered native traces confirm all three are PSMCT32 image uploads in GIF DMA
chain `0xe0418ac0`: REFS tags at stream offsets 704 (`0x64d6e0`, tag
`0xe0418bb0`), 1312 (`0x64d8e0`, tag `0xe0418c30`), and 1920 (`0x64dae0`, tag
`0xe0418cb0`), each QWC 32. Each preceding GIF setup sets `TRXREG=(16,8)` and
`TRXDIR=0` (host-to-local), then uses a GIF IMAGE tag for 32 qwords. These are
16x8 image uploads, not mesh geometry. A conditional GDB breakpoint at the
matching chain root confirmed the retail
call stack `func_001DCEA0 -> func_001B3450 -> func_001692B0 ->
func_001686E8 -> func_00202A80 -> openrac_native_renderer_submit_dma_chain`.
This is resource initialization; it does not explain the zero scene vertices.
An entry-filtered breakpoint at `func_001662A8` for `0x00847ce0` also showed
`func_00165728 -> func_00124A48 -> func_00146470 -> func_001DCEA0` as its
caller chain. The command-buffer capture therefore belongs to resource
initialization, not a per-frame scene draw. See
`/tmp/sly-dump-render-entry.gdb` and `/tmp/sly-dump-render-entry.log`.

The write-watch GDB script was `/tmp/sly-watch-mesh-first-write.gdb`; the read
watch script and output are `/tmp/sly-watch-mesh-read.gdb` and
`/tmp/sly-watch-mesh-read.log`. Both derive guest base from native symbol
address `0x724e90`. The DMA target traces are
`/tmp/sly-native-dma-target-context.log`,
`/tmp/sly-native-dma-target-64d8e0.log`, and
`/tmp/sly-native-dma-target-64dae0.log`; the conditional stack trace is
`/tmp/sly-watch-mesh-chain.gdb` and `/tmp/sly-watch-mesh-chain.log`. The
source-level reader and current packet evidence are now
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

Trace the renderable-object list and retail draw callback that should populate
the scene geometry address fields; the three image transfers above are not
the missing mesh path. Do not force the skipped groups or synthesize packet
data. Separately trace the normal
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

### Restart note (2026-11-07)

The latest corrected guest-memory dump is `/tmp/sly-dump-render-entry-guest.log`;
the earlier `/tmp/sly-dump-render-entry.log` used host addresses incorrectly
and should not be used. The corrected dump shows the real entry at
`0x00847ce0`, nested records at `0x00848070` (stride `0x50`), and shared
payload pointer `0x00840730` in each of its three nested records. The observed
call remains in initialization:
`func_001662A8 <- func_00165728 <- func_00124A48 <- func_00146470 <- func_001DCEA0`.

That suspected override mismatch has now been checked. The full retail US
assembly body (`asm/us/text.s`, around line 125800) uses the same entry stride
(`0x70`), subdata stride (`0x28`), object callback at entry `+0x38`, nested
call to `func_001662A8`, and linked-node offsets (`+0x4C`, `+0x80`) as the
decomp C. No semantic discrepancy was found in the traversal. The former
trace-heavy port wrapper has been moved to
`hand/diagnostics/func_00165728_trace.c`; it is no longer registered as a live
override. `hostgen.json` explicitly selects `func_00165728` from US retail
assembly through `from_assembly` and `asm.only`. The generated report confirms
`asm/us/text.s` as its source (223 functions selected from assembly).

A 12-second native run using that exact assembly still stays at
`sly_frame=0,3,0`, `game_mode=0`, and `vertices=0`, with three GIF chain
submissions and two active guest threads (`func_001DC710` and
`func_0015EE80`). This confirms the C wrapper was not the cause of the black
screen. The bounded run was stopped by `timeout` (exit 124); there was no
launcher crash. Its full trace is `/tmp/sly-native-165728-retail-asm.log`.
Continue by tracing why the normal startup draw/frame path does not advance
past this state and why it submits no per-frame scene or UI geometry. Keep any
functional C repair in the port-only backfill area, never in `sly1-decomp`.

Corrected dump highlights: entry `0x00847ce0` has three nested records at
`0x00848070`, `0x008480c0`, and `0x00848110`; each record points at
`0x00840730`. The entry's `+0x28` flag is zero, so retail
`func_001662A8` proceeds for this entry. These records still have not been
connected to per-frame geometry. The `func_00165728` selection mismatch is
resolved; resume tracing the actual retail render-list producer and why the
regular startup draw path does not submit geometry. The normal Splash-to-title
transition is a separate unresolved route; direct-level mode remains
diagnostic only.

No repository changes were pending at the restart checkpoint before this
note. Rebuild with `cmake --build build/port --target port_native -j2`; native
launcher runs are authorized. Do not run PCSX2 or another emulator.

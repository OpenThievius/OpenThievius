<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Sly 1 native graphics handoff

Last checked: 2026-11-06. This document records the live native C-only
launcher state, not a playable milestone. The launcher opens a window and
reaches Sly's frame/menu work, but the rendered image is black. No emulator is
used by this path.

## What runs today

Build and launch from the repository root:

```sh
cmake --build build/port --target port_native -j2
build/port/sly1_native build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

The source ELF and ISO above are already present in the current development
environment. The executable is a host-native launcher linked with SDL/OpenGL
and OpenRAC renderer code; it is not PCSX2 or another emulator. The current
experimental game path still uses guest-addressed memory and small EE DMA,
VIF1, VU1 and GS compatibility services. Those services are a bridge for
diagnosing Sly's draw stream, not the intended architecture for a finished
port. The target design remains native game code, native platform services
and Sly-specific draw-data adapters; see [DESIGN.md](DESIGN.md) and
[OPENRAC_REUSE.md](OPENRAC_REUSE.md).

The display packet path reaches the port-only backfill `func_00202A80`. An
earlier trace was incorrectly interpreted as a malformed VIF stream: the
`0x08000000` word at offset `0x54` is the start of a GIF IMAGE tag, not a VIF
opcode. The chain includes a 6-qword reference followed by an 8,192-qword GIF
image transfer. The writer initializes `D_00261F1C` from the channel table at
`0x00279340`; channel 2 in that table is GIF at `0x1000A000`, whereas VIF1 is
`0x10009000`. The backfill now attempts to send GIF channel starts directly to
the native GIF renderer and retains a separate VIF1 route. This dispatch is
still under investigation: the latest bounded run showed no GIF chain submits,
so the channel value and call path must be traced before treating the route as
verified.

The VIF1 runtime remains available for actual VIF1 work. It collects bounded
DMA chains, including references into the port heap at `0xE0000000`, interprets
VIF commands, runs the existing VU1 instruction translator on MSCAL, and
forwards DIRECT data or XGKICK output to the GIF/GS parser. Its runtime API is
listed in `port/game/sly1-us/hostgen.json` to avoid a colliding missing-function
stub.

## Current measured blocker

A bounded run on 2026-11-06 reaches the menu/frame path and produces a
135,632-byte display chain. At `0x002C3B10`, the chain references 6 qwords at
`0xE0DF9340`, then 8,192 qwords at `0xE0C02330`. The second block begins with a
GIF IMAGE tag (`0x08000000`, NLOOP 8192), which explains why the VIF parser
rejected it. The corrected channel-2 route is present in the port backfill,
but the latest trace reports `gif_chain_submits=0`; therefore the immediate
blocker is confirming the runtime `dma` argument and ensuring this call path
is active, not changing the VIF parser to accept GIF bytes.

The same run reports zero decoded vertices and zero renderer draws. GS display
register writes occur, but no usable scene geometry reaches the SDL renderer.
The four earlier 144-byte startup GIF transfers are display setup; they do
not prove that the frame's draw stream is valid. The loaded menu font and
resource state also do not prove that a visible texture upload or draw was
submitted.

To capture the failure and first words of the stream, run:

```sh
OPENRAC_VIF_TRACE=1 OPENRAC_STARTUP_TRACE=1 timeout 12s \
  build/port/sly1_native build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

## Work needed next

1. Trace the arguments at the entry to `func_00202A80` in the C-only native
   launcher. Record channel and source for each call and compare them with
   `D_00261F1C` and the channel table in the current decomp snapshot. Confirm
   whether the channel-2 branch is reached, and why the runtime currently
   counts zero submissions. Do not infer channel from the packet contents.
2. Once channel 2 reaches the GIF renderer, verify that the two REF segments
   are read at their guest addresses and that a complete GIFtag ending in EOP
   reaches `openrac_guest_gif_stream_write`. If a separate actual VIF1 chain is
   submitted, trace it independently through MSCAL, VU1 data inputs, XGKICK,
   and GIF output. Do not feed GIF IMAGE data into VIF or skip unknown bytes.
3. Get a first visible frame. Confirm a texture/image transfer and at least
   one nonempty draw in `DirectRenderer`; inspect a captured window image.
   Current zero-vertex traces fail this check. Keep renderer limitations
   explicit and add Sly-specific handling only after the emitted packet
   format is known.
4. Then continue from a visible title/menu into level loading, controller
   input, a controllable Sly, collision, audio, and saves. A menu screenshot is
   useful progress but does not satisfy the playable-state goal.

## Port/decomp boundary

All functional replacements used to reach this point belong under
`port/game/sly1-us/hand/native_c/` and its registration tables. They are
port-only backfills and must stay replaceable when matching C appears in
`sly1-decomp`. Do not edit the decompilation checkout to make the port work.
The runtime transport and renderer adapters belong under `port/runtime/` and
`port/renderer/`; document any temporary guest-device behavior as such.

## What a new contributor needs

No additional disc image or boot ELF is needed in the current environment.
The useful help is a small, reviewable investigation of the DMA channel
argument, the channel-table initialization, and the GIF renderer submission
path. A graphics/runtime contributor should be comfortable reading PS2 DMA,
GIF, VIF and VU packet formats, tracing decompiled C against port-only C
backfills, and following C++ renderer input. The repository contains the
current branch, extracted game data under ignored build paths, and a refreshed
read-only decomp snapshot at `/home/reaper/.cache/sly1-decomp-latest`; no
copyrighted disc bytes or extracted data should be committed.

To reproduce the current window and collect startup/renderer counters:

```sh
cmake --build build/port --target port_native -j2
OPENRAC_STARTUP_TRACE=1 timeout 12s build/port/sly1_native \
  build/port/extracted/boot/SCUS_971.98 \
  '/home/reaper/pcsx2/Games/Sly Cooper and the Thievius Raccoonus (USA).iso'
```

Use `OPENRAC_VIF_TRACE=1` only when investigating actual VIF1 submissions.
The ISO path above is specific to this machine; another developer can supply
their own NTSC-U boot ELF and ISO. Keep new functional game-code replacements
in `port/game/sly1-us/hand/native_c/` and record them in the backfill
inventory, so matching implementations in `sly1-decomp` can replace them
cleanly. Runtime/rendering fixes belong in `port/runtime/` or `port/renderer/`.

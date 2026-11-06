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

The native backfill `func_00202A80` now sends VIF1 channel starts to
`openrac_guest_vif1_mmio_store32`. That runtime collects bounded DMA chains,
including references into the port heap at `0xE0000000`, interprets VIF
commands, runs the existing VU1 instruction translator on MSCAL, and forwards
DIRECT data or XGKICK output to the GIF/GS parser. The VIF1 handler is listed
as a runtime-provided function in `port/game/sly1-us/hostgen.json`, so hostgen
does not emit a colliding missing-function stub.

## Current measured blocker

A bounded run on 2026-11-06 reaches the menu/frame path and submits a VIF1
DMA chain. The first display chain contains 135,632 bytes. The collector now
accepts the chain, including a REF tag at `0x002C3B10` to native port-heap data
at `0xE0DF9340`. The VIF parser stops at byte offset `0x54` on word
`0x08000000` (opcode `0x08`). That opcode is not one of the standard VIF
commands listed by PS2SDK; the surrounding words at offsets `0x00`–`0x50`
include `0x10004000` (FLUSHE) and `0x14081400` (MSCAL), followed by tag-like
values and the start of texture bytes. Do not treat opcode `0x08` as a no-op
without first establishing whether the stream was assembled or aligned
correctly.

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

1. Establish the VIF input layout. Trace the caller and writer path around
   `func_00152AE0`, `func_00152B80`, `func_00152D70`, `func_00161708` and
   `func_00202A80` against the current NTSC-U decompilation and retail
   behavior. Dump the DMA tags and the referenced `0xE0DF9340` block with
   addresses and lengths. Determine whether the 135,632-byte buffer is one
   VIF command stream, several streams that need stateful boundaries, or a
   higher-level packet with nested data. Fix the collector or parser at the
   source of any layout error; do not skip unknown bytes to force progress.
2. Make VIF execution complete. Confirm the first MSCAL's microprogram entry,
   VU1 data inputs, and XGKICK output. The evidence should show a complete
   GIFtag ending in EOP arriving at `openrac_guest_gif_stream_write`, not only
   an accepted DMA tag chain.
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
The useful handoff is the repository at its current branch plus the trace
command above. A graphics/runtime contributor should be comfortable reading
PS2 DMA/VIF/VU packet layouts, tracing decompiled C and native C backfills,
and debugging C++ renderer input. Work from the checked-in decomp snapshot
and public PS2 documentation; keep copyrighted disc bytes and extracted data
out of commits. The current unresolved question is the stream layout at
`0x002C3B10` / `0xE0DF9340`, not missing user-provided files.

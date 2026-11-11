# The native port

The port will run the decompiled game natively on Windows, macOS and Linux,
with the game's data read from your own disc. The current architecture and
staged plan are in [docs/port/DESIGN.md](../docs/port/DESIGN.md) and
[docs/port/ROADMAP.md](../docs/port/ROADMAP.md).
The [OpenRAC reuse plan](../docs/port/OPENRAC_REUSE.md) explains the native
port pattern and marks the current hardware-model launcher as experimental,
not the target gameplay architecture.

The raw-disc tools recover standalone VAG audio samples and extract the
NTSC-U executable's 46 catalogued resource ranges. The catalog tool can also
decode all ranges with the retail LZ routine. The native launcher serves exact
catalog sector ranges from the extracted files on later runs and reads other
ranges from the ISO. Usage and limits are in
[docs/port/ASSETS.md](../docs/port/ASSETS.md).

The first port tool inspects the decompilation's committed sources and reports
matching coverage:

```sh
python3 port/tools/sly1.py status --source /path/to/sly1-decomp
python3 port/tools/sly1.py status --version eu --json
```

It is a planning/status tool, not a host build. The host build and its current
limits are described in [the port roadmap](../docs/port/ROADMAP.md).

Host generation can be run with `python3 port/tools/sly1.py hostgen`; the
standalone command defaults to `--asm off`. The native CMake launcher enables
the retail assembly fallbacks listed in `game/sly1-us/hostgen.json`. Functions
with matching C are skipped automatically. The active `build/port` cache now
uses the read-only source snapshot `/tmp/sly1-decomp-782e2899-source` and US
retail assembly from `/tmp/sly1-decomp-wave24-asm`. Hostgen translates 734 C
functions and 501 assembly functions across 256 units, with 2 stubs and 3,674
functions without C. Generated C stays in the ignored build tree, separate
from matched decomp C and port backfills. Set `OPENTHIEVIUS_ASM_SOURCE_SLY1`
when the matched-source checkout has no assembly directory.

The four VIF packet-writer helpers reached by the direct-level path now use
exact US retail assembly. A bounded forced-level run still exhausts the 32 MiB
guest heap during mesh packet construction; the normal Splash run stays black
with inactive UI and no visible text. The separate `func_001BAC30` port reader
remains active because hostgen reports the upstream `LoadCollider` body as
`no-C`. See the [playable TODO](../docs/port/PLAYABLE_TODO.md) and
[restart checkpoint](../docs/port/RESTART_CHECKPOINT.md) for the current
diagnostics and remaining work.
The graphics handoff records the current evidence and next blocker. Functional
C backfills stay under `game/sly1-us/hand/native_c/`, outside `sly1-decomp`.

## Canonical decomp rework check (2026-11-10)

Canonical `OpenThievius/sly1-decomp` master is `2e00ac70` (PR #109). The
isolated `build/port/canonical-sdl` configuration uses a read-only snapshot at
`/tmp/sly1-decomp-2e00ac70` for C and the older local checkout only for its US
retail assembly. Hostgen translates 562 functions, including 252 from
assembly, with 2 stubs and 4,069 functions without C. This proves the new
layout can be consumed by the current hostgen pipeline, but its native launch
still crashes during resource loading at guest address `0xFFFF0626`; it is
not ready to replace the default launcher configuration. See
[`RESTART_CHECKPOINT.md`](../docs/port/RESTART_CHECKPOINT.md) for the trace.

To translate the selected assembly functions into the native build output
directly, hostgen also supports:

```sh
python3 port/tools/sly1.py hostgen --asm on --native-asm   --source /path/to/sly1-decomp   --asm-source /path/to/sly1-decomp-with-asm
```

Without `--native-asm`, `--asm on` writes a separate research output. In
either mode the configured `asm.only` list limits translation to the selected
functions listed in `hostgen.json`.

Port-side replacements are listed in `game/sly1-us/libraries.tsv`, with their
native implementations in the runtime. The `extra_entries` list in
`game/sly1-us/hostgen.json` identifies additional retail assembly entry points
needed by the port. A matching entry supplied by the decompilation takes
precedence over that list. Some launcher paths still contain guest-addressed
EE/DMA/VIF/VU/GS compatibility services from the earlier startup experiment;
they are diagnostic scaffolding, not the target gameplay architecture.

The current graphics task is to trace normal Splash/title activation and why
all UI elements stay inactive, then inspect the 2,128-byte UI packet's
invalid attributes. The latest native run still has zero visible vertices,
image tags, or text. A separate DMA fix now preserves VIF1's retail-set TTE
bit; it removes the malformed startup stream but has not produced graphics.
See the launch command and evidence in `docs/port/SLY_GRAPHICS_HANDOFF.md`.
Once a readable menu is visible, the port still needs the title/level
transition, Sly pad-library integration, controllable character movement and
collision, real audio, and saves. See the
[roadmap](../docs/port/ROADMAP.md) and [graphics handoff](../docs/port/SLY_GRAPHICS_HANDOFF.md)
for status and starter tasks.

OpenRAC's port design and its own authored code may be used with the credit
required by [the sourcing policy](../docs/policy/SOURCING.md#openrac). Sly's
code and behavior are researched independently from retail.

Everything here is AGPL-3.0-or-later ([LICENSE.md](../LICENSE.md)).

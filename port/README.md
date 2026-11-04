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
decode all ranges with the retail LZ routine. Usage and limits are in
[docs/port/ASSETS.md](../docs/port/ASSETS.md).

The first port tool inspects the decompilation's committed sources and reports
matching coverage:

```sh
python3 port/tools/sly1.py status --source /path/to/sly1-decomp
python3 port/tools/sly1.py status --version eu --json
```

It is a planning/status tool, not a host build. The host build and its current
limits are described in [the port roadmap](../docs/port/ROADMAP.md).

Host generation can be run with `python3 port/tools/sly1.py hostgen`; its
default is `--asm off`, and generated files stay under `build/port/`. This
generates host C from matched Sly sources plus port-only backfills. It does not
translate retail MIPS assembly. The current report translates 1,526 functions
and lists 3,210 catalogued functions without C; 15 of 68 source units remain
unreadable to its C frontend. The SDL native launcher
builds and opens a window, reaches Sly's display loop, and submits GIF work,
but the renderer sees no visible vertices and the window remains black. The
separate freestanding host32 launcher is a startup diagnostic and does not
open a window. SDL keyboard/gamepad input now updates host-side DualShock
reports, but the game's pad-library calls are not connected to them yet.
Backfills are kept under `game/sly1-us/hand/native_c/`, outside
`sly1-decomp`.

`python3 port/tools/sly1.py hostgen --asm on` writes a separate
`build/port/hostgen/us-asm-research/` tree. That mode translates retail MIPS
instructions for bounded research only; it is not the playable path. The
target architecture is compiled Sly game logic with native platform services
and a Sly-specific renderer, reusing OpenRAC's generic host/runtime
infrastructure where it fits. See [`OPENRAC_REUSE.md`](../docs/port/OPENRAC_REUSE.md).

The reused guest runtime builds as `sly1_port_runtime`. To compile and link all
generated game units into a local link-check executable, run:

```sh
cmake -S port -B build/port -DOPENTHIEVIUS_SOURCE_SLY1=/path/to/sly1-decomp
cmake --build build/port --target port_compile_generated
cmake --build build/port --target port_link_generated
cmake --build build/port --target port_native
```

The link check confirms that the generated C and shared runtime link together;
it does not call the game entry. `port_native` builds
`build/port/sly1_native` from the same no-assembly host output. The port-only
boot entry reaches a backfilled frame loop, but does not yet reach gameplay.
The optional
`--asm on` research output is separate and must not be used as a gameplay
dependency.

Port-side replacements are listed in `game/sly1-us/libraries.tsv`, with their
native implementations in the runtime. The `extra_entries` list in
`game/sly1-us/hostgen.json` identifies additional retail assembly entry points
needed by the port. A matching entry supplied by the decompilation takes
precedence over that list.
The launcher now accepts Sly's `SetupThread` stack range and registers five
interior constructor callbacks. Native ISO file lookup and reads, a CD-ready
bootstrap hook, validated EE RAM DMA copies, interrupt registration, timer
reads, and counting semaphores support more of startup. Guest worker threads
run on host threads with their own guest stacks. SIF register reads and writes
are stored in host memory, and the remote IOP RPC bind loop is bypassed when an
ISO is present. Native startup publishes the SIF BOOTEND and CMDINIT flags
(register 4, value 0x60000) and an opaque subaddress when the disc-backed IOP
replacement starts. Native command handling completes the SIF command-init
handshake and binds the two guest RPC clients. A retail-assembly translation
for `func_001B93D8` preserves callback registers that its C placeholder lost.
The latest bounded run reads the initial ISO sectors, passes the first sound
service request using a silent fallback and reaches the display loop. The
port-side VIF/VU1 path now tracks TOP/TOPS double buffering and executes integer
loads from VU data memory correctly. It submits valid XGKICK packets from the
first geometry path, then reaches a later XGKICK whose address points into
non-tag data. The renderer still reports zero visible vertices, so the SDL
window remains black.
The native memory-card boundary answers the observed extended-server probe
(`0x80000400`, RPC `0xFE`) with a zero result structure; retail's
`func_002093F0` rejects the missing version field and returns `-0x78`. This
does not implement card detection or saves. Guest threads continue issuing GIF
work, but the color-only renderer does not yet draw the game's textures or
produce a title screen. The native sound boundary completes RPC 77 as idle, so
the game continues without audio playback. Textured rendering, Sly pad-library
integration, real audio, saves, SIF DMA and remaining device services are not
implemented.
Hostgen translates the entry's BSS setup
and observed boot syscalls (`SetupThread`, `SetupHeap`, `ExitThread`,
`Exit`, `SetGsCrt`, `Copy`,
`GetEntryAddress`, `FlushCache`, and `SetSyscall`). The
`GetMemorySize` and `_InitTLB` boot calls are also handled for the 32 MiB guest
RAM configuration; TLB writes are ignored because the runtime maps guest RAM
directly. Guest worker threads run on host threads, and semaphores block and
signal across them. OSD configuration currently defaults to English.
Sly's CP0 timer read is backed by a monotonic 294.912 MHz host clock.
The missing retail timer snapshot at `func_001430C8` is replaced in the port
runtime and updates Sly's extended timer globals using two host clock samples.
One startup VU busy poll reports idle. The port-side VU1 translator supports
only a partial instruction set; the remaining microcode behavior is not yet
implemented.
The interrupt controller stores installed handlers and enable state; the
renderer can raise those events through a host API, but no renderer events are
wired yet. CP0 interrupt-status reads reflect the host interrupt-enable state.
The startup path also saves and changes the GS interrupt mask through a
software-backed guest register. The renderer, frame loop, remaining disc
services and other required platform services are not implemented, so the
port still cannot launch into gameplay.

What it will need, in order ([docs/port/ROADMAP.md](../docs/port/ROADMAP.md)):

1. Resolve remaining boot RPCs and identify the transition from startup into
   the title and frame paths.
2. Implement the Sly renderer and host services for input, audio, files and
   saves.
3. Add asset extraction/conversion and platform packaging as their needs become
   concrete.

OpenRAC's port design and its own authored code may be used with the credit
required by [the sourcing policy](../docs/policy/SOURCING.md#openrac). Sly's
code and behavior are researched independently from retail.

Everything here is AGPL-3.0-or-later ([LICENSE.md](../LICENSE.md)).

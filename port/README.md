# The native port

The port will run the decompiled game natively on Windows, macOS and Linux,
with the game's data read from your own disc. The current architecture and
staged plan are in [docs/port/DESIGN.md](../docs/port/DESIGN.md) and
[docs/port/ROADMAP.md](../docs/port/ROADMAP.md).

The first port tool inspects the decompilation's committed sources and reports
matching coverage:

```sh
python3 port/tools/sly1.py status --source /path/to/sly1-decomp
python3 port/tools/sly1.py status --version eu --json
```

It is a planning/status tool, not a host build. The next step is the source
bridge and host build described in [the port roadmap](../docs/port/ROADMAP.md).

Host generation can be tried with `python3 port/tools/sly1.py hostgen`; its
generated files stay under `build/port/`. The current translation is
experimental and does not produce a playable game. It needs full C++ source
handling, a port runtime, and a host implementation of the boot/system calls.

The reused guest runtime builds as `sly1_port_runtime`. To compile and link all
generated game units into a local link-check executable, run:

```sh
cmake -S port -B build/port -DOPENTHIEVIUS_SOURCE_SLY1=/path/to/sly1-decomp
cmake --build build/port --target port_compile_generated
cmake --build build/port --target port_link_generated
cmake --build build/port --target port_native
```

The link check confirms that generated game code and the guest runtime link
together. It registers the generated function table if launched, but never calls
the game entry. `port_native` separately builds `build/port/sly1_native`, a
launcher that accepts the extracted `SCUS_971.98` ELF and an optional ISO or
raw BIN disc image, loads the ELF segments into guest memory, registers
translated functions, and calls the game entry. The native runtime validates
ISO 9660 images, reads logical sectors, locates paths and reads file ranges.
The IOP bootstrap constructor now uses a native hook that sets CD-ready state
when an ISO is loaded; the file-open poll uses ISO path lookup. The native
launcher is experimental.
The launcher now accepts Sly's `SetupThread` stack range and registers five
interior constructor callbacks. Native ISO file lookup and reads, a CD-ready
bootstrap hook, validated EE RAM DMA copies, interrupt registration, timer
reads, and counting semaphores support more of startup. Guest worker threads
run on host threads with their own guest stacks. SIF register reads and writes
are stored in host memory, and the remote IOP RPC bind loop is bypassed when an
ISO is present. Native startup publishes the SIF BOOTEND and CMDINIT flags
(register 4, value 0x60000) and an opaque subaddress when the disc-backed IOP
replacement starts. Native command handling completes the SIF command-init
handshake and binds the two guest RPC clients. The latest 15-second run reached
the first sound RPC request (service `0x00123456`, RPC 0) and passed it with
an empty completion reply. The main thread then polls around `0x00203828`.
The native runtime still has no audio backend or SIF DMA command
dispatcher, so startup has not reached a title screen or gameplay. The renderer,
frame loop, input, audio, saves and remaining disc/device services are also
unimplemented.
Hostgen translates the entry's BSS setup
and observed boot syscalls (`SetupThread`, `SetupHeap`, `ExitThread`,
`Exit`, `SetGsCrt`, `Copy`,
`GetEntryAddress`, `FlushCache`, and `SetSyscall`). The
`GetMemorySize` and `_InitTLB` boot calls are also handled for the 32 MiB guest
RAM configuration; TLB writes are ignored because the runtime maps guest RAM
directly. Worker threads are recorded but do not run separately; Sema calls
use a single-thread counting model. OSD configuration currently defaults to
English.
Sly's CP0 timer read is backed by a monotonic 294.912 MHz host clock.
One startup VU busy poll reports idle because the host has no VU execution unit.
The interrupt controller stores installed handlers and enable state; the
renderer can raise those events through a host API, but no renderer events are
wired yet. CP0 interrupt-status reads reflect the host interrupt-enable state.
The startup path also saves and changes the GS interrupt mask through a
software-backed guest register. The renderer, frame loop, remaining disc
services and other required platform services are not implemented, so the
port still cannot launch into gameplay.

What it will need, in order ([docs/port/ROADMAP.md](../docs/port/ROADMAP.md)):

1. Implement the SIF DMA and IOP RPC responses required to advance boot.
2. Trace the title and frame paths, then implement the Sly renderer and host
   services for input, audio, files and saves.
3. Add asset extraction/conversion and platform packaging as their needs become
   concrete.

OpenRAC's port design and its own authored code may be used with the credit
required by [the sourcing policy](../docs/policy/SOURCING.md#openrac). Sly's
code and behavior are researched independently from retail.

Everything here is AGPL-3.0-or-later ([LICENSE.md](../LICENSE.md)).

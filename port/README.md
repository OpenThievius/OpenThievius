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
The first authorized startup run stopped at the runtime's stack-bound check
and five interior constructor table entries. The runtime now accepts Sly's
`SetupThread` stack range, and the launcher registers
those five callback addresses to call their translated registration routines.
A second authorized startup run passed those points and stayed alive in the
CD/IOP initialization path until its timeout. Later runs with native ISO file
open remained alive through further initialization until timeout. The fourth
authorized run, with native DMA span handling, ran to its 15-second timeout
without a crash; it reported missing VU initialization and formatted logging.
Native interrupt registration was implemented afterward, but no renderer events
are wired yet. Hostgen now translates more
interior entries used by that path. EE interrupt disable/enable helpers
preserve nested critical-section state and return the prior enable bit. The
retail DMA span helper copies validated guest RAM spans immediately and reports
them complete. Transfers to non-RAM devices still need native device services.
The data-cache range helper is replaced with a host memory fence, and the EE
serial diagnostic print is discarded on the host. The translated boot frontier
now has 14 functions without C, down from 16; formatted buffer output and the
remaining device services still need work.
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

1. Host-build scaffolding that reads `sly1-decomp` without modifying it and
   reports the functions still blocking the execution path.
2. An extractor and asset converters derived from Sly 1 retail discs.
3. A native renderer and host services for audio, input, files and saves.

OpenRAC's port design and its own authored code may be used with the credit
required by [the sourcing policy](../docs/policy/SOURCING.md#openrac). Sly's
code and behavior are researched independently from retail.

Everything here is AGPL-3.0-or-later ([LICENSE.md](../LICENSE.md)).

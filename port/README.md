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

What it will need, in order ([docs/port/ROADMAP.md](../docs/port/ROADMAP.md)):

1. Host-build scaffolding that reads `sly1-decomp` without modifying it and
   reports the functions still blocking the execution path.
2. An extractor and asset converters derived from Sly 1 retail discs.
3. A native renderer and host services for audio, input, files and saves.

OpenRAC's port design and its own authored code may be used with the credit
required by [the sourcing policy](../docs/policy/SOURCING.md#openrac). Sly's
code and behavior are researched independently from retail.

Everything here is AGPL-3.0-or-later ([LICENSE.md](../LICENSE.md)).

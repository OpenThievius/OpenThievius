# The native port

Not started. The port will run the decompiled game natively on Windows, macOS
and Linux, with the game's data read from your own disc.

What it will need, in order ([docs/ROADMAP.md](../docs/ROADMAP.md)):

1. A design note (`docs/port/`): how decompiled PS2 code is hosted on PC, what
   replaces the EE, VU, GS, IOP and SPU2, and how the port stays in step with
   sly1-decomp as functions are matched. OpenRAC's `port/` is the model, and
   its code may be used, credited ([SOURCING.md](../docs/policy/SOURCING.md#openrac));
   nothing comes from other ports.
2. An extractor for the disc's data.
3. The runtime, the renderer, audio, input and saves.

Everything here is AGPL-3.0-or-later ([LICENSE.md](../LICENSE.md)).

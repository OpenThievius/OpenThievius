# Superseded backfills

Files here are kept as port history and are not staged by `tools/sly1.py hostgen`. Do not register them as active backfills. Prefer exact retail
assembly or matching decompilation C when available.

`resource_object_record_loader.c` was an instrumented guess for
`func_0011ECB8`. Exact NTSC-U assembly shows that function reads an object's
callback list; its count is unrelated to scene object or mesh counts. The old
source also had an unverified byte-alignment heuristic. It is retained only
to explain earlier traces and has been removed from the active build.

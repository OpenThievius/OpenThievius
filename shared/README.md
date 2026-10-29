# shared: files a game project keeps a copy of

A project has to build on its own, so a file it needs from here is copied into
it rather than referenced. [files.json](files.json) lists each original here
and its copies; `python3 tools/openthievius.py check` fails when a copy
differs. Change the original and every copy in the same change.

| Original | Copies |
|---|---|
| `tools/disc.py` | sly1-decomp `tools/disc.py` |

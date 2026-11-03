# hostgen

This is OpenRAC's GPL-3.0-or-later host-generation tool, adapted for the Sly
1 source layout. Its upstream notices are preserved; see
[THIRD_PARTY_NOTICES.md](../../../THIRD_PARTY_NOTICES.md). The shared
`tools/openrac_common/` package is included for its table and path readers.

Hostgen rewrites decompilation sources for a 64-bit native host and can
translate remaining functions from a contributor's local retail-backed
assembly. Generated output belongs under ignored `build/port/`.

The Sly adapter is experimental. Sly's C++ files are staged as C-compatible
input for the current frontend. That handles many source files but does not
preserve every C++ construct, so the generated program is not playable yet.
The report lists unreadable units, stubs and the reachable frontier. Do not
use this output as a matching build or change the decompilation through this
tool.

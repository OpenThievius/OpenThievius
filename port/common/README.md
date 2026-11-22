# Common support

`common` holds small host-side facilities shared by the port: logging, crash
reports, environment-variable access, process statistics, profiling, JSON,
PS2 floating-point helpers, and utility code. It does not contain Sly game
logic or a renderer.

## Main interfaces

| Header | Purpose |
|---|---|
| `log.h`, `line_ring.h` | Session logging and a bounded recent-log buffer |
| `crash.h`, `breadcrumbs.h`, `crash_symbols.h` | Crash reports, activity breadcrumbs, and best-effort symbol lookup |
| `env.h` | The table of `OPENRAC_*` variables read by the port |
| `stats.h`, `profiler.h`, `perf_capture.h` | Runtime counters, optional profiling, and performance captures |
| `system_info.h`, `build_info.h`, `process.h` | Host/build details and process measurements |
| `json.h`, `zip_writer.h` | JSON parsing and ZIP output |
| `ps2_float.h`, `float_encoding.h` | PS2-compatible float operations and compact float encodings |
| `result.h`, `c_boundary.h`, `joining_thread.h`, `parallel.h` | Error results, C-boundary exception handling, and thread helpers |

`env.cpp` is the source of truth for the environment-variable table and each
variable's default and description. This repository has no separate debugging
or profiling guide; use the current [port status](../README.md) and
[graphics handoff](../../docs/port/SLY_GRAPHICS_HANDOFF.md) for the active
native-port work.

## Scope and dependencies

This layer is shared host support. Higher-level runtime, renderer, platform,
and game code may depend on it; `common` should not depend on those layers.
Build targets and dependencies are declared in [`../CMakeLists.txt`](../CMakeLists.txt).

The port does not currently define the unit-test targets described in older
versions of this page. Do not treat those historical test names as available
verification commands.

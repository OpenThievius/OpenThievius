# Tauri launcher

The desktop launcher lives in `launcher/`. It uses a Rust and Tauri 2 backend
with a Svelte 5 interface. Rust owns disc recognition, setup, configuration,
data movement, and launching. The interface calls discrete Tauri commands and
listens for scan and setup events.

## Current support

Sly 1 has NTSC-U and PAL retail definitions. The launcher reads the registry in
`games/sly1/game.json` and checks its IDs, roles, boot paths, and SHA-1 values
against `../sly1-decomp/config/versions.json` during the Rust build when that
checkout is present. Reference versions are omitted from installable versions.
If another game's registry and project definitions are added, the launcher
build discovers them the same way. It does not embed prototype entries.

Sly 2 and Sly 3 have navigation pages and reserved data directories, but this
repository does not yet contain their maintained retail definitions or native
ports. Their pages therefore show that version definitions are not available.

## Development

Install Rust, Node.js, npm, and the Tauri Linux development packages. Tauri's
Linux prerequisites include GTK 3 and WebKitGTK development libraries. From
`launcher/`, install the frontend dependencies and start the app:

```sh
npm install
npm run tauri -- dev
```

Build the frontend and Rust bundle with:

```sh
npm run check
npm run build
npm run tauri -- build --bundles appimage
```

`packaging/build-appimage.sh` runs the AppImage build and prints the generated
artifact path. The AppImage and all other build products are under the ignored
`launcher/target/` directory. The first release target is x86-64 Linux
AppImage; Windows and macOS packaging remain future work.

## Data and setup

The default data root is the platform's standard application data directory
under `openthievius` (on Linux, `~/.local/share/openthievius`). It contains
`config.json`, launcher files, and a `sly1/`, `sly2/`, and `sly3/` directory;
each game directory has a separate `saves/` folder. The first configuration
uses JSON and can be edited by hand. The `playable` map defaults to `true` for
missing game IDs. Setup state is tracked independently in `installed`.

The launcher scans `.iso`, `.img`, and `.bin` files recursively. An extension
only determines which files to inspect. Recognition reads `SYSTEM.CNF`, opens
its boot path, and compares the boot executable's SHA-1 with a supported retail
definition. A matched retail executable is written to the user's data root as
`<game>/<version>/boot.elf`; no disc data is written to this repository. The
prototype is not embedded as an installable version.

Setup progress and individual scan results use Tauri events. The saved disc
folder is scanned after startup and when the user selects **Scan now**. A data
directory move stages a complete copy, hashes the copied files to verify it,
switches the saved root pointer, and only then removes the old directory. A
failed copy leaves the old data in place.

## Native port launch contract

The launcher starts the configured game-port executable with two arguments:
the verified local `boot.elf` path and the selected source image path. It also
sets these environment variables:

| Variable | Value |
|---|---|
| `OPENTHIEVIUS_GAME_ID` | `sly1`, `sly2`, or `sly3` |
| `OPENTHIEVIUS_GAME_DATA_DIR` | That game's data directory |
| `OPENTHIEVIUS_SAVE_DIR` | That game's `saves/` directory |
| `OPENTHIEVIUS_DISPLAY_MODE` | `windowed` or `borderless` |
| `OPENTHIEVIUS_WINDOW_WIDTH` | `1280` |
| `OPENTHIEVIUS_WINDOW_HEIGHT` | `720` |

The display mode and default window dimensions are launcher preferences for the
native port to consume. The launcher does not emulate the game or provide a
native port executable itself. Play requires a verified setup, a configured
port executable, and a `playable` value other than `false`.

## Source and packaging constraints

Launcher code is AGPL-3.0-or-later. The Rust disc reader and configuration
logic are project code; they use no game or third-party source. Disc images,
extracted executables, and build outputs stay outside version control. Do not
weaken `tools/proprietary.py` to package local game data.

# Launcher

The launcher lives in `launcher/`. It uses Rust and GPUI. At build time, it
validates every game's retail version IDs, boot names, boot SHA-1 values,
roles, and primary version against that project's maintained version file.
That checkout is required: the build fails instead of silently using stale
umbrella data when it is missing. Display labels come from
`games/*/game.json`. Reference versions are omitted from scan definitions.
The app does not invoke Python at runtime. The AppImage CI build checks out
`OpenThievius/sly1-decomp` and validates the launcher definitions against its
maintained version table.

Build and run from the repository root:

```sh
cargo build --locked --release --manifest-path launcher/Cargo.toml
cargo run --locked --manifest-path launcher/Cargo.toml
```

Clone the matching game project beside this repository first, as shown in
the root README, or set its `OPENTHIEVIUS_SOURCE_*` environment variable.

On Linux, install the development libraries used by GPUI before building:
`libfontconfig1-dev`, `libfreetype-dev`, `libvulkan-dev`, `libwayland-dev`,
`libxkbcommon-dev`, and `libxkbcommon-x11-dev` on Debian or Ubuntu. The launcher
CI job installs these packages and builds the Linux AppImage. GPUI selects
Wayland when `WAYLAND_DISPLAY` is set and falls back to X11 when only
`DISPLAY` is available. The package retains both backends; X11 libraries are
not required by a Wayland session.

To build the first-release Linux package, install Python 3, `curl`,
`squashfs-tools`, and those same GPUI development libraries, then run:

```sh
launcher/packaging/build-appimage.sh
```

The script stages only the launcher and its desktop icon, downloads
`linuxdeploy` into a temporary directory, extracts its static executable
without running the AppImage wrapper, and writes
`launcher/target/appimage/dist/OpenThievius-x86_64.AppImage`. It does not add
disc images, game executables, or extracted game files. CI runs the packaging
step on Ubuntu 22.04 and saves the AppImage as a workflow artifact. It also
builds a portable Windows zip and a macOS `.app` zip. Build either package on
its native platform with `launcher/packaging/build-windows.ps1` or
`launcher/packaging/build-macos.sh`; outputs go under
`launcher/target/<platform>/dist/`. Those packages are not signed and do not
include installers.

Run the built AppImage from the repository root with:

```sh
APPIMAGELAUNCHER_DISABLE=1 ./launcher/target/appimage/dist/OpenThievius-x86_64.AppImage
```

`APPIMAGELAUNCHER_DISABLE=1` starts the AppImage directly when AppImageLauncher
is installed, without invoking its integration handler.

The app recursively scans `.iso`, `.img`, and `.bin` files in the chosen folder
using the embedded retail boot definitions. It reads the ISO9660 `SYSTEM.CNF`
boot path and executable SHA-1; filenames and extensions alone do not establish
a match. It supports plain 2048-byte ISO sectors and raw 2352-byte Mode 1/Mode 2
sectors. Found supported versions can be set up by extracting the user's boot
executable to the per-user data folder. Setup reports its verification and
write stages, and the detected image path and setup state are stored in
`config.json`. Before launching, the app rechecks the selected disc's boot
path and hash and verifies the extracted executable again. This catches a disc
image or setup file changed after the scan.

The data root defaults to the platform's standard per-user application data
location. On Linux it is `~/.local/share/openthievius`; Windows and macOS use
their standard application data folders. Settings can move the data folder.
The launcher copies and verifies its contents before switching the saved
location and removing the old folder. If switching the pointer fails, the original and
verified copy are preserved and their paths are reported. If old-folder cleanup
fails after switching, the new folder remains active and the UI reports the
location of the old copy. A small pointer
under the platform's standard config location records a non-default data
folder. Invalid JSON settings are preserved under a timestamped backup name
and the launcher starts with defaults. An invalid data-root pointer is also
preserved under a timestamped backup name; the standard data folder is used
and the backup path is shown on startup.

If the data folder or settings cannot be opened, the launcher still opens and
shows the filesystem error in its status area rather than exiting before the
window appears. Settings offers a Retry storage action so the launcher can
reload the data folder and settings after the filesystem problem is fixed.

The Settings page stores per-game playability, display mode, and native port
executable paths in `config.json`. Missing playability entries follow the
plan's default and permit Play after setup. Play still requires the matching
native port executable, and stays disabled until one is available. Choose the
executable in Settings; the `OPENTHIEVIUS_SLY1_EXECUTABLE` environment variable
and a `sly1_native` beside the launcher are fallbacks. For AppImage runs, a
`sly1_native` beside the AppImage file is also discovered. The launcher passes
the extracted executable, original disc image, and selected display mode to
the Sly 1 native port. It also passes `OPENTHIEVIUS_GAME_ID`,
`OPENTHIEVIUS_GAME_DATA_DIR`, and `OPENTHIEVIUS_SAVE_DIR` so each native port
can keep runtime data and saves under the matching game's directory. The
current Sly 1 port does not yet implement save/load. Sly 2 and Sly 3 can use
the same definitions and launch flow when their game definitions and native
ports are added.

The JSON config can be edited while the launcher is closed. Its game-specific
maps use `sly1`, `sly2`, and `sly3` keys; for example:

```json
{
  "disc_folder": "/home/user/Games/Discs",
  "playable": { "sly1": true, "sly2": false },
  "display_mode": {
    "sly1": "windowed",
    "sly2": "borderless_fullscreen"
  },
  "game_ports": { "sly1": "/home/user/Games/sly1_native" }
}
```

Omitted config keys use defaults. `playable` defaults to `true` for each game.

Sly 1's current native port is still experimental. Setup remains available
independently of Play. The launcher does not embed prototype definitions. This
checkout currently has supported retail definitions for Sly 1 only; the Sly 2
and Sly 3 pages are ready to consume their own definitions when those projects
provide them. Repository policy prohibits prototype-derived data; consequently
this launcher cannot positively recognize and name a prototype image without
conflicting with that policy.

Windows and macOS packaging remain future work. Keep game version recognition
aligned with each game's maintained project definitions.

#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
launcher_dir="$repo_root/launcher"
output_dir="$launcher_dir/target/appimage/dist"
appdir="$launcher_dir/target/appimage/OpenThievius.AppDir"
tool_dir="$(mktemp -d)"
cleanup() {
    rm -rf "$tool_dir"
}
trap cleanup EXIT

linuxdeploy_path="${LINUXDEPLOY_PATH:-$tool_dir/linuxdeploy-x86_64.AppImage}"
if [[ -z "${LINUXDEPLOY_PATH:-}" ]]; then
    curl --fail --location --silent --show-error \
        https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage \
        --output "$linuxdeploy_path"
fi
chmod +x "$linuxdeploy_path"

# Extract linuxdeploy without executing its AppImage wrapper. This avoids
# desktop AppImage handlers and still lets the static linuxdeploy binary run.
linuxdeploy_binary="$linuxdeploy_path"
squashfs_offset=""
while IFS=: read -r candidate _; do
    if unsquashfs -s -offset "$candidate" "$linuxdeploy_path" >/dev/null 2>&1; then
        squashfs_offset="$candidate"
        break
    fi
done < <(grep -aob 'hsqs' "$linuxdeploy_path" || true)

if [[ -n "$squashfs_offset" ]]; then
    extracted_tool="$tool_dir/linuxdeploy-root"
    unsquashfs -quiet -no-progress -offset "$squashfs_offset" \
        -d "$extracted_tool" "$linuxdeploy_path"
    linuxdeploy_binary="$extracted_tool/usr/bin/linuxdeploy"
fi
if [[ ! -x "$linuxdeploy_binary" ]]; then
    printf 'Could not find an executable linuxdeploy binary in %s\n' "$linuxdeploy_path" >&2
    exit 1
fi

cargo build --locked --release --manifest-path "$launcher_dir/Cargo.toml"
rm -rf "$appdir" "$output_dir"
mkdir -p "$appdir/usr/bin" "$output_dir"
cp "$launcher_dir/target/release/openthievius-launcher" "$appdir/usr/bin/openthievius-launcher"
cp "$launcher_dir/resources/openthievius.png" "$appdir/openthievius.png"
mkdir -p "$appdir/usr/share/metainfo"
cp "$launcher_dir/resources/org.openthievius.launcher.appdata.xml" \
    "$appdir/usr/share/metainfo/org.openthievius.launcher.appdata.xml"

LDAI_OUTPUT="$output_dir/OpenThievius-x86_64.AppImage" \
    "$linuxdeploy_binary" \
    --appdir "$appdir" \
    --executable "$appdir/usr/bin/openthievius-launcher" \
    --desktop-file "$launcher_dir/resources/org.openthievius.launcher.desktop" \
    --icon-file "$launcher_dir/resources/openthievius.svg" \
    --output appimage

test -s "$output_dir/OpenThievius-x86_64.AppImage"
chmod +x "$output_dir/OpenThievius-x86_64.AppImage"
printf 'Built %s\n' "$output_dir/OpenThievius-x86_64.AppImage"

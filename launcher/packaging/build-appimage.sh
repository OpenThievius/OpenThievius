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
tool_root=""
if [[ -z "${LINUXDEPLOY_PATH:-}" ]]; then
    linuxdeploy_asset_url="$(
        curl --fail --location --silent --show-error \
            --retry 5 --retry-all-errors --retry-delay 2 --retry-max-time 90 \
            https://api.github.com/repos/linuxdeploy/linuxdeploy/releases/tags/continuous \
            | python3 -c 'import json, sys; assets=json.load(sys.stdin)["assets"]; print(next(asset["url"] for asset in assets if asset["name"] == "linuxdeploy-x86_64.AppImage"))'
    )"
    curl --fail --location --silent --show-error \
        --retry 5 --retry-all-errors --retry-delay 2 --retry-max-time 90 \
        --header 'Accept: application/octet-stream' \
        "$linuxdeploy_asset_url" \
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
    tool_root="$extracted_tool"
    linuxdeploy_binary="$extracted_tool/usr/bin/linuxdeploy"
else
    tool_root="$(cd "$(dirname "$linuxdeploy_path")/../.." && pwd)"
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

"$linuxdeploy_binary" \
    --appdir "$appdir" \
    --executable "$appdir/usr/bin/openthievius-launcher" \
    --desktop-file "$launcher_dir/resources/org.openthievius.launcher.desktop" \
    --icon-file "$launcher_dir/resources/openthievius.svg"

appimagetool_binary="$tool_root/plugins/linuxdeploy-plugin-appimage/usr/bin/appimagetool"
if [[ ! -x "$appimagetool_binary" ]]; then
    appimagetool_binary="$(command -v appimagetool || true)"
fi
if [[ -z "$appimagetool_binary" || ! -x "$appimagetool_binary" ]]; then
    printf 'Could not find appimagetool in the linuxdeploy bundle or PATH\n' >&2
    exit 1
fi
runtime_file="${APPIMAGE_RUNTIME_FILE:-}"
if [[ -z "$runtime_file" && -n "$squashfs_offset" ]]; then
    runtime_file="$tool_dir/runtime-x86_64"
    head -c "$squashfs_offset" "$linuxdeploy_path" > "$runtime_file"
    chmod +x "$runtime_file"
fi
runtime_args=()
if [[ -n "$runtime_file" ]]; then
    runtime_args+=(--runtime-file "$runtime_file")
fi
"$appimagetool_binary" --no-appstream "${runtime_args[@]}" \
    "$appdir" "$output_dir/OpenThievius-x86_64.AppImage"

test -s "$output_dir/OpenThievius-x86_64.AppImage"
chmod +x "$output_dir/OpenThievius-x86_64.AppImage"
printf 'Built %s\n' "$output_dir/OpenThievius-x86_64.AppImage"

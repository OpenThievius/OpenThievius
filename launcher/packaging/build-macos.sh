#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
launcher_dir="$repo_root/launcher"
package_root="$launcher_dir/target/macos"
app_bundle="$package_root/OpenThievius.app"
contents="$app_bundle/Contents"
dist_dir="$package_root/dist"

cargo build --locked --release --manifest-path "$launcher_dir/Cargo.toml"
rm -rf "$app_bundle" "$dist_dir"
mkdir -p "$contents/MacOS" "$dist_dir"
cp "$launcher_dir/target/release/openthievius-launcher" \
    "$contents/MacOS/openthievius-launcher"

cat > "$contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>openthievius-launcher</string>
    <key>CFBundleIdentifier</key>
    <string>org.openthievius.launcher</string>
    <key>CFBundleName</key>
    <string>OpenThievius</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>0.1.0</string>
    <key>CFBundleVersion</key>
    <string>0.1.0</string>
</dict>
</plist>
PLIST

case "$(uname -m)" in
    arm64 | aarch64) architecture="arm64" ;;
    x86_64) architecture="x86_64" ;;
    *) printf 'Unsupported macOS architecture: %s\n' "$(uname -m)" >&2; exit 1 ;;
esac

archive="$dist_dir/OpenThievius-macos-$architecture.zip"
ditto -c -k --sequesterRsrc --keepParent \
    "$app_bundle" "$archive"
printf 'Built %s\n' "$archive"

#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root/launcher"
npm ci
npm run tauri -- build --bundles appimage

artifact_dir="$repo_root/launcher/target/release/bundle/appimage"
artifact="$(find "$artifact_dir" -maxdepth 1 -type f -name '*.AppImage' -print -quit)"
if [[ -z "$artifact" || ! -s "$artifact" ]]; then
    printf 'Tauri did not produce an AppImage in %s\n' "$artifact_dir" >&2
    exit 1
fi
chmod +x "$artifact"
printf 'Built %s\n' "$artifact"

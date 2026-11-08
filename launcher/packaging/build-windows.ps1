# SPDX-License-Identifier: AGPL-3.0-or-later
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$launcherDir = Join-Path $repoRoot 'launcher'
$releaseDir = Join-Path $launcherDir 'target/release'
$packageDir = Join-Path $launcherDir 'target/windows/OpenThievius'
$distDir = Join-Path $launcherDir 'target/windows/dist'
$executable = Join-Path $releaseDir 'openthievius-launcher.exe'
$archive = Join-Path $distDir 'OpenThievius-windows-x86_64.zip'

cargo build --locked --release --manifest-path (Join-Path $launcherDir 'Cargo.toml')
if ($LASTEXITCODE -ne 0) {
    throw "cargo build failed with exit code $LASTEXITCODE"
}

if (Test-Path $packageDir) {
    Remove-Item -Recurse -Force $packageDir
}
if (Test-Path $distDir) {
    Remove-Item -Recurse -Force $distDir
}
New-Item -ItemType Directory -Force -Path $packageDir, $distDir | Out-Null
Copy-Item $executable (Join-Path $packageDir 'OpenThievius.exe')
Compress-Archive -Path (Join-Path $packageDir '*') -DestinationPath $archive
Write-Output "Built $archive"

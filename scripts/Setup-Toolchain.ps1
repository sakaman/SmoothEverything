[CmdletBinding()]
param(
    [switch]$IncludeNetworkWorkaround,
    [switch]$IncludeInstaller
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if (-not (Get-Command scoop -ErrorAction SilentlyContinue)) {
    throw 'Scoop is required. Install Scoop first, then rerun this script.'
}

$packages = @(
    'cmake',
    'ninja',
    'mingw-mstorsjo-llvm-ucrt'
)
if (-not (Get-Command dotnet -ErrorAction SilentlyContinue)) {
    $packages += 'dotnet-sdk'
}
if ($IncludeNetworkWorkaround) {
    $packages += 'aria2'
}

foreach ($package in $packages) {
    & scoop install $package
    if ($LASTEXITCODE -ne 0) {
        throw "Scoop failed to install $package."
    }
}

if ($IncludeInstaller) {
    $installerManifest = Join-Path $PSScriptRoot 'scoop\innosetup.json'
    & scoop install $installerManifest
    if ($LASTEXITCODE -ne 0) {
        throw 'Scoop failed to install Inno Setup from the pinned project manifest.'
    }
}

Write-Host 'SmoothEverything toolchain is ready.' -ForegroundColor Green

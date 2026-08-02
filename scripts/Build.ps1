[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string]$Version = '0.1.3',
    [switch]$SkipTests
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$scoopRoot = if ($env:SCOOP) { $env:SCOOP } else { Join-Path $env:USERPROFILE 'scoop' }
$toolPaths = @(
    (Join-Path $scoopRoot 'apps\mingw-mstorsjo-llvm-ucrt\current\bin'),
    (Join-Path $scoopRoot 'apps\cmake\current\bin'),
    (Join-Path $scoopRoot 'apps\ninja\current')
) | Where-Object { Test-Path -LiteralPath $_ }
if ($toolPaths.Count -gt 0) {
    $env:PATH = ($toolPaths -join [System.IO.Path]::PathSeparator) + [System.IO.Path]::PathSeparator + $env:PATH
}

foreach ($command in @('cmake', 'ninja', 'clang++')) {
    if (-not (Get-Command $command -ErrorAction SilentlyContinue)) {
        throw "Missing $command. Run scripts\Setup-Toolchain.ps1 first."
    }
}

$preset = $Configuration.ToLowerInvariant()

Push-Location $repositoryRoot
try {
    & cmake --preset $preset "-DSE_VERSION=$Version"
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
    & cmake --build --preset $preset
    if ($LASTEXITCODE -ne 0) { throw 'Native build failed.' }

    if (-not $SkipTests) {
        & ctest --preset $preset
        if ($LASTEXITCODE -ne 0) { throw 'Native tests failed.' }
    }
}
finally {
    Pop-Location
}

Write-Host "SmoothEverything $Configuration build passed." -ForegroundColor Green

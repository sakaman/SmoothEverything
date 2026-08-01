[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [switch]$SkipRestore,
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

foreach ($command in @('cmake', 'ninja', 'clang++', 'dotnet')) {
    if (-not (Get-Command $command -ErrorAction SilentlyContinue)) {
        throw "Missing $command. Run scripts\Setup-Toolchain.ps1 first."
    }
}

$env:DOTNET_CLI_HOME = Join-Path $repositoryRoot '.dotnet-home'
$env:NUGET_PACKAGES = Join-Path $repositoryRoot '.nuget\packages'
$env:DOTNET_NOLOGO = '1'
$preset = $Configuration.ToLowerInvariant()
$settingsProject = Join-Path $repositoryRoot 'src\settings\SmoothEverything.Settings.csproj'
$modelTests = Join-Path $repositoryRoot 'tests\settings\SmoothEverything.Settings.ModelTests.csproj'
$nugetConfig = Join-Path $repositoryRoot 'NuGet.Config'
$localFeed = Join-Path $repositoryRoot '.nuget-feed'

Push-Location $repositoryRoot
try {
    & cmake --preset $preset
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
    & cmake --build --preset $preset
    if ($LASTEXITCODE -ne 0) { throw 'Native build failed.' }

    if (-not $SkipRestore) {
        $restoreArguments = @('restore', $settingsProject, '--configfile', $nugetConfig)
        if (Test-Path -LiteralPath (Join-Path $localFeed 'microsoft.windowsappsdk.1.8.260317003.nupkg')) {
            $restoreArguments += @('--source', $localFeed)
        }
        & dotnet @restoreArguments
        if ($LASTEXITCODE -ne 0) { throw 'WinUI dependency restore failed.' }
    }

    & dotnet build $settingsProject -c $Configuration -p:Platform=x64 --no-restore --nologo
    if ($LASTEXITCODE -ne 0) { throw 'Control panel build failed.' }

    $enginePath = Join-Path $repositoryRoot "artifacts\build\$preset\src\engine\SmoothEverything.Engine.exe"
    $controlPanelOutput = Join-Path $repositoryRoot "src\settings\bin\x64\$Configuration\net10.0-windows10.0.26100.0\win-x64"
    Copy-Item -LiteralPath $enginePath -Destination $controlPanelOutput -Force

    if (-not $SkipTests) {
        & ctest --preset $preset
        if ($LASTEXITCODE -ne 0) { throw 'Native tests failed.' }
        & dotnet run --project $modelTests -c $Configuration --nologo
        if ($LASTEXITCODE -ne 0) { throw 'Settings model tests failed.' }
    }
}
finally {
    Pop-Location
}

Write-Host "SmoothEverything $Configuration build passed." -ForegroundColor Green

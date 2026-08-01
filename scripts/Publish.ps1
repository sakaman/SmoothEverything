[CmdletBinding()]
param(
    [switch]$FrameworkDependent
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
& (Join-Path $PSScriptRoot 'Build.ps1') -Configuration Release
if ($LASTEXITCODE -ne 0) {
    throw 'Release build failed.'
}

$env:DOTNET_CLI_HOME = Join-Path $repositoryRoot '.dotnet-home'
$env:NUGET_PACKAGES = Join-Path $repositoryRoot '.nuget\packages'
$env:DOTNET_NOLOGO = '1'
$settingsProject = Join-Path $repositoryRoot 'src\settings\SmoothEverything.Settings.csproj'
$nugetConfig = Join-Path $repositoryRoot 'NuGet.Config'
$localFeed = Join-Path $repositoryRoot '.nuget-feed'
$publishDirectory = [System.IO.Path]::GetFullPath((Join-Path $repositoryRoot 'artifacts\publish\win-x64'))
$publishRoot = [System.IO.Path]::GetFullPath((Join-Path $repositoryRoot 'artifacts\publish'))
if (-not $publishDirectory.StartsWith($publishRoot + [System.IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Resolved publish directory escaped the expected workspace path.'
}
if (Test-Path -LiteralPath $publishDirectory) {
    Remove-Item -LiteralPath $publishDirectory -Recurse -Force
}
New-Item -ItemType Directory -Path $publishDirectory -Force | Out-Null

$selfContained = if ($FrameworkDependent) { 'false' } else { 'true' }
$restoreArguments = @(
    'restore',
    $settingsProject,
    '--runtime', 'win-x64',
    "-p:SelfContained=$selfContained",
    '--configfile', $nugetConfig
)
if (Test-Path -LiteralPath (Join-Path $localFeed 'microsoft.windowsappsdk.1.8.260317003.nupkg')) {
    $restoreArguments += @('--source', $localFeed)
}
& dotnet @restoreArguments
if ($LASTEXITCODE -ne 0) {
    throw 'Publish dependency restore failed.'
}

& dotnet publish $settingsProject `
    -c Release `
    -r win-x64 `
    --self-contained $selfContained `
    -p:Platform=x64 `
    -p:PublishTrimmed=false `
    -o $publishDirectory `
    --no-restore `
    --nologo
if ($LASTEXITCODE -ne 0) {
    throw 'Control panel publish failed.'
}

Copy-Item `
    -LiteralPath (Join-Path $repositoryRoot 'artifacts\build\release\src\engine\SmoothEverything.Engine.exe') `
    -Destination $publishDirectory `
    -Force
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'README.md') -Destination $publishDirectory -Force

$archivePath = Join-Path $repositoryRoot 'artifacts\SmoothEverything-0.1.0-win-x64.zip'
Compress-Archive -Path (Join-Path $publishDirectory '*') -DestinationPath $archivePath -CompressionLevel Optimal -Force

Write-Host "Published: $publishDirectory" -ForegroundColor Green
Write-Host "Archive:   $archivePath" -ForegroundColor Green

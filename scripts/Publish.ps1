[CmdletBinding()]
param(
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string]$Version = '0.1.1',
    [switch]$FrameworkDependent,
    [switch]$SkipInstaller
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
& (Join-Path $PSScriptRoot 'Build.ps1') -Configuration Release -Version $Version
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
    "-p:Version=$Version" `
    "-p:FileVersion=$Version.0" `
    "-p:AssemblyVersion=$Version.0" `
    "-p:InformationalVersion=$Version" `
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

$archivePath = Join-Path $repositoryRoot "artifacts\SmoothEverything-$Version-win-x64.zip"
Compress-Archive -Path (Join-Path $publishDirectory '*') -DestinationPath $archivePath -CompressionLevel Optimal -Force

$releaseAssets = [System.Collections.Generic.List[string]]::new()
$releaseAssets.Add($archivePath)
$installerPath = $null

if (-not $SkipInstaller) {
    $scoopRoot = if ($env:SCOOP) { $env:SCOOP } else { Join-Path $env:USERPROFILE 'scoop' }
    $programFilesX86 = [Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFilesX86)
    $isccCommand = Get-Command 'ISCC.exe' -ErrorAction SilentlyContinue
    $isccCandidates = @(
        if ($isccCommand) { $isccCommand.Source }
        (Join-Path $scoopRoot 'apps\innosetup-np\current\ISCC.exe')
        (Join-Path $scoopRoot 'apps\innosetup\current\ISCC.exe')
        if ($programFilesX86) { Join-Path $programFilesX86 'Inno Setup 6\ISCC.exe' }
        if ($env:ProgramFiles) { Join-Path $env:ProgramFiles 'Inno Setup 7\ISCC.exe' }
    ) | Where-Object { $_ -and (Test-Path -LiteralPath $_) }
    $isccPath = $isccCandidates | Select-Object -First 1
    if (-not $isccPath) {
        throw 'Inno Setup compiler not found. Run scripts\Setup-Toolchain.ps1 -IncludeInstaller first.'
    }

    $installerScript = Join-Path $repositoryRoot 'installer\SmoothEverything.iss'
    & $isccPath `
        '/Qp' `
        "/DAppVersion=$Version" `
        "/DPublishSource=$publishDirectory" `
        "/O$(Join-Path $repositoryRoot 'artifacts')" `
        $installerScript
    if ($LASTEXITCODE -ne 0) {
        throw 'Installer compilation failed.'
    }

    $installerPath = Join-Path $repositoryRoot "artifacts\SmoothEverything-Setup-$Version-x64.exe"
    if (-not (Test-Path -LiteralPath $installerPath)) {
        throw "Installer output was not created: $installerPath"
    }
    $releaseAssets.Add($installerPath)
}

$checksumPath = Join-Path $repositoryRoot 'artifacts\SHA256SUMS.txt'
[string[]]$checksumLines = @(
    $releaseAssets |
        Sort-Object { [System.IO.Path]::GetFileName($_) } |
        ForEach-Object {
            $hash = (Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash.ToLowerInvariant()
            "$hash  $([System.IO.Path]::GetFileName($_))"
        }
)
[System.IO.File]::WriteAllLines(
    $checksumPath,
    $checksumLines,
    [System.Text.UTF8Encoding]::new($false))

Write-Host "Published: $publishDirectory" -ForegroundColor Green
Write-Host "Archive:   $archivePath" -ForegroundColor Green
if ($installerPath) {
    Write-Host "Installer: $installerPath" -ForegroundColor Green
}
Write-Host "Checksums: $checksumPath" -ForegroundColor Green

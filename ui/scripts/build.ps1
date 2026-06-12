[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [switch]$Fresh
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptDir

$configurePreset = if ($Configuration -eq "Release") { "vcpkg-release" } else { "vcpkg-debug" }
$buildPreset = if ($Configuration -eq "Release") { "release" } else { "debug" }

Push-Location $repoRoot
try {
    Write-Host "Configuring with preset '$configurePreset'..."
    if ($Fresh) {
        & cmake --preset $configurePreset --fresh
    }
    else {
        & cmake --preset $configurePreset
    }
    if ($LASTEXITCODE -ne 0) {
        throw "cmake configure failed with exit code $LASTEXITCODE."
    }

    Write-Host "Building with preset '$buildPreset'..."
    & cmake --build --preset $buildPreset
    if ($LASTEXITCODE -ne 0) {
        throw "cmake build failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}

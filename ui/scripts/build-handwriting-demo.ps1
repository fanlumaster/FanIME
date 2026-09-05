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
    $configureArguments = @("--preset", $configurePreset)
    if ($Fresh) { $configureArguments += "--fresh" }
    & cmake @configureArguments
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed with exit code $LASTEXITCODE." }
    & cmake --build --preset $buildPreset --target msimeui-handwriting-demo
    if ($LASTEXITCODE -ne 0) { throw "Handwriting demo build failed with exit code $LASTEXITCODE." }
}
finally {
    Pop-Location
}

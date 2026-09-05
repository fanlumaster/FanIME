param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Set-Location $projectRoot

Write-Host "[TsfEditControl] Configuring project..." -ForegroundColor Cyan
cmake --preset vcpkg
if ($LASTEXITCODE -ne 0) {
    throw "cmake configure failed with exit code $LASTEXITCODE"
}

$buildPreset = $Configuration.ToLowerInvariant()
Write-Host "[TsfEditControl] Building ($Configuration)..." -ForegroundColor Cyan
cmake --build --preset $buildPreset
if ($LASTEXITCODE -ne 0) {
    throw "cmake build failed with exit code $LASTEXITCODE"
}

Write-Host "[TsfEditControl] Build completed." -ForegroundColor Green

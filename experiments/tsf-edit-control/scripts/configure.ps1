param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Set-Location $projectRoot

Write-Host "[TsfEditControl] Configuring project only..." -ForegroundColor Cyan
cmake --preset vcpkg
if ($LASTEXITCODE -ne 0) {
    throw "cmake configure failed with exit code $LASTEXITCODE"
}

Write-Host "[TsfEditControl] Configure completed for $Configuration." -ForegroundColor Green

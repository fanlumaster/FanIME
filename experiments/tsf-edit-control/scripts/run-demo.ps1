param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Set-Location $projectRoot

$buildScript = Join-Path $PSScriptRoot 'build.ps1'
& $buildScript -Configuration $Configuration
if ($LASTEXITCODE -ne 0) {
    throw "build step failed with exit code $LASTEXITCODE"
}

$exePath = Join-Path $projectRoot "build\bin\$Configuration\TsfEditControlDemo.exe"
if (-not (Test-Path $exePath)) {
    throw "demo executable not found: $exePath"
}

Write-Host "[TsfEditControl] Launching demo..." -ForegroundColor Cyan
Start-Process -FilePath $exePath

[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptDir

$demoPath = Join-Path $repoRoot "build/bin/$Configuration/msimeui-demo.exe"

Push-Location $repoRoot
try {
    if (-not (Test-Path -LiteralPath $demoPath)) {
        throw "Demo executable not found: $demoPath"
    }

    Write-Host "Running $demoPath"
    Start-Process -FilePath $demoPath
}
finally {
    Pop-Location
}

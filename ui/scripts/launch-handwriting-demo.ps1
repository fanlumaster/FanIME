[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [switch]$Build
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptDir

if ($Build) {
    & (Join-Path $scriptDir "build-handwriting-demo.ps1") -Configuration $Configuration
}

$demoPath = Join-Path $repoRoot "build/bin/$Configuration/msimeui-handwriting-demo.exe"
if (-not (Test-Path -LiteralPath $demoPath)) {
    throw "Handwriting demo executable not found: $demoPath. Run build-handwriting-demo.ps1 first or pass -Build."
}

Start-Process -FilePath $demoPath -WorkingDirectory $repoRoot

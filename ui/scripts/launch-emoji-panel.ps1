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
    & (Join-Path $scriptDir "build-emoji-panel.ps1") -Configuration $Configuration
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

$panelPath = Join-Path $repoRoot "build/bin/$Configuration/msimeui-emoji-panel.exe"
if (-not (Test-Path -LiteralPath $panelPath)) {
    throw "Emoji panel executable not found: $panelPath. Run build-emoji-panel.ps1 first or pass -Build."
}

Start-Process -FilePath $panelPath -WorkingDirectory $repoRoot

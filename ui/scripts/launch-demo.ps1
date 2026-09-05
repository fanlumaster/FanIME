[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [switch]$Fresh
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildScript = Join-Path $scriptDir "build.ps1"
$runScript = Join-Path $scriptDir "run-demo.ps1"

& $buildScript -Configuration $Configuration -Fresh:$Fresh
& $runScript -Configuration $Configuration

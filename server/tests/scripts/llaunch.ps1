param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug'
)
$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'lcompile.ps1') -Configuration $Configuration
& (Join-Path $PSScriptRoot 'lrun.ps1') -Configuration $Configuration

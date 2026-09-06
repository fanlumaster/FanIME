$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'lcompile-release.ps1') 32
& (Join-Path $PSScriptRoot 'lcompile-release.ps1') 64

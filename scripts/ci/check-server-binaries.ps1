# Fail early when the Server build is missing something the installer collects, rather than
# letting Prepare-PackageFiles.ps1 report it several jobs later.
param([string]$BuildDir = 'build/bin/Release')

$ErrorActionPreference = 'Stop'

$required = @(
    'MetasequoiaImeServer.exe',
    'MetasequoiaImeSettings.exe',
    'MetasequoiaImeWatchdog.exe',
    'MetasequoiaImeDictionaryReplay.exe'
)

$missing = $required | Where-Object { -not (Test-Path (Join-Path $BuildDir $_)) }
if ($missing) {
    throw "Missing from the Server build: $($missing -join ', ')"
}

Get-ChildItem $BuildDir -File | Select-Object Name, Length | Format-Table

# Build the TSF DLL without depending on the caller's working directory.
[CmdletBinding()]
param([ValidateSet('32', '64')][string]$Architecture = '64')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot "build$Architecture-release"
Push-Location $projectRoot
try {
    cmake "--preset=for$Architecture-release"
    if ($LASTEXITCODE -ne 0) { throw "TSF $Architecture configure failed ($LASTEXITCODE)" }
    cmake --build $buildDirectory --config Release
    if ($LASTEXITCODE -ne 0) { throw "TSF $Architecture build failed ($LASTEXITCODE)" }
} finally { Pop-Location }

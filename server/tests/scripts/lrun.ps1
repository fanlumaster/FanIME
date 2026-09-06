param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug'
)
$ErrorActionPreference = 'Stop'
$serverRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$buildDirectory = Join-Path $serverRoot $(if ($Configuration -eq 'Release') { 'build-release' } else { 'build' })
# Uses the caller's prepared product data; never launch the interactive Server host.
ctest --test-dir $buildDirectory -C $Configuration --output-on-failure --no-tests=error --timeout 120
if ($LASTEXITCODE -ne 0) { throw "Server tests failed ($LASTEXITCODE)" }

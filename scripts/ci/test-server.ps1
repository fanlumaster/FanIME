param(
    [Parameter(Mandatory = $true)][string]$ServerRoot,
    [Parameter(Mandatory = $true)][string]$StagingRoot,
    [string]$BuildDir = 'build'
)
$ErrorActionPreference = 'Stop'
$automationRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$server = (Resolve-Path $ServerRoot).Path
# The helpcodes moved into the engine, so the gitlink that pins the contracts pins the tables too.
# verify-contracts is what keeps that gitlink and the lock from drifting apart.
$helpcodes = Join-Path $automationRoot 'vendor/MetasequoiaImeEngine/helpcode/helpcodes'
if (-not (Test-Path -LiteralPath $helpcodes -PathType Container)) {
    throw "Helpcodes not found at $helpcodes; run git submodule update --init"
}
python (Join-Path $automationRoot 'scripts/product_lock.py') verify-contracts $automationRoot
if ($LASTEXITCODE -ne 0) { throw 'The engine submodule does not match the product lock' }
python (Join-Path $automationRoot 'scripts/product_lock.py') fetch-dictionaries --staging-root $StagingRoot
if ($LASTEXITCODE -ne 0) { throw 'Could not provision locked dictionaries' }
$verified = (Resolve-Path (Join-Path $StagingRoot 'MetasequoiaImeDict/out')).Path
# Some Windows-only helpers still resolve LOCALAPPDATA; isolate those and the
# cross-platform engine under the same root so no installed dictionary is used.
$env:LOCALAPPDATA = Join-Path (Resolve-Path $StagingRoot).Path 'user-local'
$data = Join-Path $env:LOCALAPPDATA 'metasequoiaime'
New-Item -ItemType Directory -Force -Path $data | Out-Null
Copy-Item (Join-Path $verified '*') -Destination $data -Force
Copy-Item $helpcodes -Destination $data -Recurse -Force
Copy-Item (Join-Path $server 'assets/tables/*') -Destination $data -Force
Copy-Item (Join-Path $server 'assets/config/config.toml') -Destination $data -Force
$env:METASEQUOIA_IME_DATA_DIR = $data
ctest --test-dir (Join-Path $server $BuildDir) -C Release --output-on-failure --timeout 120
if ($LASTEXITCODE -ne 0) { throw 'Server integration tests failed for the locked product' }
& (Join-Path $PSScriptRoot 'probe-server.ps1') -ServerRoot $server -BuildDir $BuildDir

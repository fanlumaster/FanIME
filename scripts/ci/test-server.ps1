param(
    [Parameter(Mandatory = $true)][string]$ServerRoot,
    [Parameter(Mandatory = $true)][string]$HelpcodeRoot,
    [Parameter(Mandatory = $true)][string]$StagingRoot,
    [string]$BuildDir = 'build'
)
$ErrorActionPreference = 'Stop'
$automationRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$server = (Resolve-Path $ServerRoot).Path
python (Join-Path $automationRoot 'scripts/product_lock.py') verify-checkout server $server
if ($LASTEXITCODE -ne 0) { throw 'Server does not match the product lock' }
python (Join-Path $automationRoot 'scripts/product_lock.py') verify-checkout helpcode $HelpcodeRoot
if ($LASTEXITCODE -ne 0) { throw 'Helpcodes do not match the product lock' }
python (Join-Path $automationRoot 'scripts/product_lock.py') fetch-dictionaries --staging-root $StagingRoot
if ($LASTEXITCODE -ne 0) { throw 'Could not provision locked dictionaries' }
$verified = (Resolve-Path (Join-Path $StagingRoot 'MetasequoiaImeDict/out')).Path
# Some Windows-only helpers still resolve LOCALAPPDATA; isolate those and the
# cross-platform engine under the same root so no installed dictionary is used.
$env:LOCALAPPDATA = Join-Path (Resolve-Path $StagingRoot).Path 'user-local'
$data = Join-Path $env:LOCALAPPDATA 'metasequoiaime'
New-Item -ItemType Directory -Force -Path $data | Out-Null
Copy-Item (Join-Path $verified '*') -Destination $data -Force
Copy-Item (Join-Path $HelpcodeRoot 'helpcodes') -Destination $data -Recurse -Force
Copy-Item (Join-Path $server 'assets/tables/*') -Destination $data -Force
Copy-Item (Join-Path $server 'assets/config/config.toml') -Destination $data -Force
$env:METASEQUOIA_IME_DATA_DIR = $data
ctest --test-dir (Join-Path $server $BuildDir) -C Release --output-on-failure --timeout 120
if ($LASTEXITCODE -ne 0) { throw 'Server integration tests failed for the locked product' }

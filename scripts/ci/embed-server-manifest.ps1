# Embed the Server's uiAccess manifest before the binary is uploaded or signed.
# Authenticode signs the exact PE bytes, so this must run before sign-binaries.ps1.
[CmdletBinding()]
param(
    [string]$BuildDir = 'server/build-release/bin/Release',
    [string]$ManifestPath = 'server/MetasequoiaImeServer.manifest',
    [string]$ManifestTool
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$buildRoot = (Resolve-Path (Join-Path $repoRoot $BuildDir)).Path
$binary = Join-Path $buildRoot 'MetasequoiaImeServer.exe'
$manifest = (Resolve-Path (Join-Path $repoRoot $ManifestPath)).Path

if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) {
    throw "Server binary not found: $binary"
}

if ([string]::IsNullOrWhiteSpace($ManifestTool)) {
    $ManifestTool = (Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\bin\*\x64\mt.exe' |
        Sort-Object FullName -Descending | Select-Object -First 1).FullName
}

$manifestCommand = if ([string]::IsNullOrWhiteSpace($ManifestTool)) {
    $null
}
else {
    Get-Command -Name $ManifestTool -ErrorAction SilentlyContinue
}
if ([string]::IsNullOrWhiteSpace($ManifestTool) -or
    (-not $manifestCommand -and -not (Test-Path -LiteralPath $ManifestTool -PathType Leaf))) {
    throw 'mt.exe not found in the Windows SDK.'
}

Write-Host "Embedding $manifest into $binary"
& $ManifestTool -manifest $manifest "-outputresource:$binary;1"
if ($LASTEXITCODE -ne 0) {
    throw "Server manifest embedding failed ($LASTEXITCODE)"
}

# Read the resource back through mt.exe so a successful process exit cannot hide a
# wrong resource target or a malformed manifest. The temporary file never enters the
# package and is removed even when validation fails.
$probe = Join-Path ([IO.Path]::GetTempPath()) ("msime-server-manifest-" + [Guid]::NewGuid() + '.xml')
try {
    & $ManifestTool "-inputresource:$binary;#1" "-out:$probe"
    if ($LASTEXITCODE -ne 0) {
        throw "Server manifest verification failed ($LASTEXITCODE)"
    }
    $embedded = Get-Content -LiteralPath $probe -Raw
    if ($embedded -notmatch 'uiAccess\s*=\s*["'']true["'']') {
        throw 'Embedded Server manifest does not enable uiAccess.'
    }
    Write-Host 'Verified embedded Server manifest: uiAccess=true.'
}
finally {
    Remove-Item -LiteralPath $probe -Force -ErrorAction SilentlyContinue
}

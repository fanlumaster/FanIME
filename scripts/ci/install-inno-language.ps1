# Put the Chinese language file next to the Inno Setup compiler.
#
# msime_setup.iss declares a chinesesimplified language pointing at
# compiler:Languages\ChineseSimplified.isl, and the Inno Setup on the runner does not ship that
# file, so ISCC aborts on the [Languages] section. Pinned by commit and checksum rather than
# tracking a branch, so the installer's Chinese text cannot change under a rebuild.
#
# Requires ISL_URL and ISL_SHA256.
$ErrorActionPreference = 'Stop'

$iscc = Get-Command 'ISCC.exe' -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
$root = if ($iscc) { Split-Path $iscc.Source -Parent } else { 'C:\Program Files (x86)\Inno Setup 6' }
$languages = Join-Path $root 'Languages'
$target = Join-Path $languages 'ChineseSimplified.isl'

if (Test-Path $target) {
    Write-Host "Already present: $target"
    exit 0
}

New-Item -ItemType Directory -Path $languages -Force | Out-Null
Invoke-WebRequest -Uri $env:ISL_URL -OutFile $target -MaximumRetryCount 3 -RetryIntervalSec 5

$actual = (Get-FileHash $target -Algorithm SHA256).Hash.ToLower()
if ($actual -ne $env:ISL_SHA256) {
    Remove-Item $target -Force
    throw "ChineseSimplified.isl checksum mismatch: expected $env:ISL_SHA256, got $actual"
}

Write-Host "Installed $target ($((Get-Item $target).Length) bytes)"

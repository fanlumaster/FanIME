# Sign the given files with the release certificate from the repository secrets.
#
# msime-installer's Sign-PackageBinaries-Local.ps1 mints a self-signed certificate for local
# testing and must never run in CI, so this is a separate path using the real certificate. Used
# for the uiAccess Server binary and the finished installer.
#
# Requires CERTIFICATE_BASE64, CERTIFICATE_PASSWORD, TIMESTAMP_URL.
param(
    [Parameter(Mandatory)][string[]]$Path,
    [switch]$Recurse
)

$ErrorActionPreference = 'Stop'

$targets = if ($Recurse) {
    Get-ChildItem -Recurse -File -Include '*.exe', '*.dll' -Path $Path
}
else {
    Get-Item -Path $Path
}

# A package can contain the same payload through multiple input paths. Sign each
# physical file at most once; certificate usage is per signed file, not per
# signtool invocation.
$targets = @($targets | Sort-Object FullName -Unique)

if (-not $targets) {
    throw "Nothing to sign under: $($Path -join ', ')"
}

Write-Host "Signing $($targets.Count) unique file(s):"
$targets | ForEach-Object { Write-Host " - $($_.FullName)" }

$signtool = Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\bin\*\x64\signtool.exe' |
    Sort-Object FullName -Descending | Select-Object -First 1
if (-not $signtool) { throw 'signtool.exe not found in the Windows SDK.' }

if ($env:CERTIFICATE_THUMBPRINT) {
    $thumbprint = $env:CERTIFICATE_THUMBPRINT -replace '\s', ''
    $cert = Get-ChildItem Cert:\CurrentUser\My\$thumbprint -ErrorAction SilentlyContinue
    if ($cert -and $cert.HasPrivateKey) {
        & $signtool.FullName sign /sha1 $thumbprint /fd sha256 /tr $env:TIMESTAMP_URL /td sha256 /v @($targets.FullName)
        if ($LASTEXITCODE -ne 0) { throw "signtool failed with exit code $LASTEXITCODE" }
        Write-Host "Signed using certificate store thumbprint $thumbprint."
        return
    }
    if (-not ($env:CERTIFICATE_BASE64 -and $env:CERTIFICATE_PASSWORD)) {
        throw "Certificate with thumbprint $thumbprint is not available with a private key, and no PFX fallback is configured."
    }
    Write-Warning "Certificate thumbprint $thumbprint is unavailable; falling back to the PFX secret."
}

if ($env:CERTIFICATE_BASE64 -and $env:CERTIFICATE_PASSWORD) {
    $pfx = Join-Path $env:RUNNER_TEMP 'metasequoia-signing.pfx'
    [IO.File]::WriteAllBytes($pfx, [Convert]::FromBase64String($env:CERTIFICATE_BASE64))
    try {
        & $signtool.FullName sign /f $pfx /p $env:CERTIFICATE_PASSWORD /fd sha256 /tr $env:TIMESTAMP_URL /td sha256 /v @($targets.FullName)
        if ($LASTEXITCODE -ne 0) { throw "signtool failed with exit code $LASTEXITCODE" }
    } finally { Remove-Item $pfx -Force -ErrorAction SilentlyContinue }
}
else {
    throw 'No usable signing certificate is configured.'
}

Write-Host "Signed $($targets.Count) file(s)."

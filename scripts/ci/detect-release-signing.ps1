# Decide whether this release can be signed, and say so in the log either way.
#
# Signing is optional so the pipeline stays runnable without a certificate, but an unsigned build
# is degraded in a way users will notice: per AGENTS.md, uiAccess="true" only takes effect for a
# correctly signed binary installed somewhere Windows trusts, so the candidate window cannot float
# over elevated hosts. Unsigned artifacts therefore carry a suffix and the release notes say so.
#
# Writes signing_enabled and asset_suffix to GITHUB_OUTPUT.
$ErrorActionPreference = 'Stop'

$storeCert = $null
if ($env:CERTIFICATE_THUMBPRINT) {
    $normalized = $env:CERTIFICATE_THUMBPRINT -replace '\s', ''
    $storeCert = Get-ChildItem Cert:\CurrentUser\My\$normalized -ErrorAction SilentlyContinue
    if (-not $storeCert -or -not $storeCert.HasPrivateKey) { $storeCert = $null }
    if (-not $storeCert -and -not ($env:CERTIFICATE_BASE64 -and $env:CERTIFICATE_PASSWORD)) {
        throw "A signing certificate thumbprint is configured, but the certificate or private key is unavailable in the runner user store."
    }
}

if ($storeCert -or ($env:CERTIFICATE_BASE64 -and $env:CERTIFICATE_PASSWORD)) {
    "signing_enabled=true" >> $env:GITHUB_OUTPUT
    "asset_suffix=" >> $env:GITHUB_OUTPUT
    if ($storeCert) { Write-Host 'Signing certificate present in the runner user certificate store.' }
    else { Write-Host 'Signing certificate secret present: the installer and its binaries will be signed.' }
}
else {
    "signing_enabled=false" >> $env:GITHUB_OUTPUT
    "asset_suffix=-unsigned" >> $env:GITHUB_OUTPUT
    Write-Host '::warning::No signing certificate configured. Publishing an unsigned installer; uiAccess will not take effect.'
}

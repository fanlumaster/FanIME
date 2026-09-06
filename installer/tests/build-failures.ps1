# Exercise the real orchestration with synthetic command failures; never sign or install.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$source = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('msime build failures ' + [Guid]::NewGuid())
$originalLocation = Get-Location
try {
    foreach ($component in @('windows', 'server')) {
        $scripts = Join-Path $fixture "$component/scripts"
        New-Item -ItemType Directory -Force $scripts | Out-Null
        Copy-Item (Join-Path $source "$component/scripts/lcompile-release.ps1") $scripts
    }
    function global:cmake {
        $global:MsimeBuildCalls += 1
        $global:LASTEXITCODE = if ($global:MsimeBuildCalls -eq $global:MsimeFailAt) { 17 } else { 0 }
    }
    foreach ($component in @('windows', 'server')) {
        foreach ($failure in @(1, 2)) {
            $global:MsimeBuildCalls = 0
            $global:MsimeFailAt = $failure
            $rejected = $false
            try { & (Join-Path $fixture "$component/scripts/lcompile-release.ps1") }
            catch { $rejected = $_.Exception.Message -match '(configure|build) failed \(17\)' }
            if (-not $rejected) { throw "$component did not reject failed build stage $failure" }
            if ($global:MsimeBuildCalls -ne $failure) { throw 'Continued after a failed native command' }
        }
    }
    function global:Invoke-MsimeManifestProbe {
        $global:MsimeManifestArguments = @($args)
        $global:LASTEXITCODE = $global:MsimeManifestExit
    }
    $global:MsimeFailAt = 0
    $global:MsimeManifestExit = 0
    & (Join-Path $fixture 'server/scripts/lcompile-release.ps1') -ManifestTool Invoke-MsimeManifestProbe
    $binary = Join-Path $fixture 'server/build-release/bin/Release/MetasequoiaImeServer.exe'
    if ($global:MsimeManifestArguments.Count -ne 3 -or $global:MsimeManifestArguments[2] -ne "-outputresource:$binary;1") {
        throw 'Manifest resource id or path with spaces was split into separate commands'
    }
    $global:MsimeManifestExit = 31
    $rejected = $false
    try { & (Join-Path $fixture 'server/scripts/lcompile-release.ps1') -ManifestTool Invoke-MsimeManifestProbe }
    catch { $rejected = $_.Exception.Message -eq 'Server manifest embedding failed (31)' }
    if (-not $rejected) { throw 'Failed manifest embedding was accepted' }
    Remove-Item Function:/cmake
    # Each installer entry must stop before staging/signing when the settings build fails.
    foreach ($relative in @('windows/scripts/lcompile-release-both.ps1', 'server/scripts/lcompile-release.ps1')) {
        [IO.File]::WriteAllText((Join-Path $fixture $relative), '# successful synthetic component build')
    }
    $settings = Join-Path $fixture 'ui-html/webview2/settings/ime-settings'
    New-Item -ItemType Directory -Force $settings | Out-Null
    [IO.File]::WriteAllText((Join-Path $settings 'package.json'), '{}')
    $installer = Join-Path $fixture 'installer'
    New-Item -ItemType Directory -Force $installer | Out-Null
    [IO.File]::WriteAllText((Join-Path $installer 'Prepare-PackageFiles.ps1'), "throw 'Reached package staging after failure'")
    function global:pnpm { $global:LASTEXITCODE = 23 }
    foreach ($entry in @('test.ps1', 'test-light.ps1')) {
        Copy-Item (Join-Path $source "installer/$entry") $installer
        $rejected = $false
        try { & (Join-Path $installer $entry) }
        catch { $rejected = $_.Exception.Message -eq 'Settings page build failed (23)' }
        if (-not $rejected) { throw "$entry continued after the settings build failed" }
    }
    $global:LASTEXITCODE = 0
    Write-Host 'Configure/build failures stop local packaging before staging, signing or installation'
} finally {
    Set-Location $originalLocation
    Remove-Item Function:/cmake, Function:/pnpm, Function:/Invoke-MsimeManifestProbe -ErrorAction SilentlyContinue
    Remove-Variable MsimeBuildCalls, MsimeFailAt, MsimeManifestExit, MsimeManifestArguments -Scope Global -ErrorAction SilentlyContinue
    if (Test-Path $fixture) { Remove-Item $fixture -Recurse -Force }
}

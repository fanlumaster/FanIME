param(
    [Parameter(Mandatory = $true)][string]$ServerRoot,
    [string]$BuildDir = 'build'
)
$ErrorActionPreference = 'Stop'
$automationRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$server = (Resolve-Path $ServerRoot).Path
$binary = Join-Path $server "$BuildDir/bin/Release/MetasequoiaImeServer.exe"
# The engine is a submodule of the repository, not of the server, so the contracts do not move with
# -ServerRoot.
$contracts = Join-Path $automationRoot 'vendor/MetasequoiaImeEngine/contracts'
# An include directory that does not exist is not a configure error; it surfaces minutes later as
# C1083 on a header nobody moved. Say what is actually wrong, before building anything.
if (-not (Test-Path -LiteralPath (Join-Path $contracts 'ipc_negotiation.h') -PathType Leaf)) {
    throw "Engine contracts not found at $contracts; run git submodule update --init"
}
$probes = @()
foreach ($architecture in @('Win32', 'x64')) {
    $output = Join-Path $env:RUNNER_TEMP "msime-pipe-probe-$architecture"
    cmake -S (Join-Path $automationRoot 'tests/pipe-probe') -B $output -A $architecture "-DMSIME_CONTRACTS_DIR=$contracts"
    if ($LASTEXITCODE -ne 0) { throw "Probe configure failed: $architecture" }
    cmake --build $output --config Release --parallel
    if ($LASTEXITCODE -ne 0) { throw "Probe build failed: $architecture" }
    $probes += Join-Path $output 'Release/msime-ipc-probe.exe'
}
# CI owns the process lifetime. The existing marker prevents the watchdog from
# restarting it after the probe; no DLL is registered and no IME is installed.
$process = Start-Process -FilePath $binary -ArgumentList '--watchdog-managed --pipe-probe' `
    -WorkingDirectory (Split-Path $binary) -PassThru
try {
    foreach ($probe in $probes) {
        & $probe
        if ($LASTEXITCODE -ne 0) { throw "Native pipe probe failed: $probe" }
    }
} finally {
    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit(10000) | Out-Null
    }
}

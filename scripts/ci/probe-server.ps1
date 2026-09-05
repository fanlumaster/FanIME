param(
    [Parameter(Mandatory = $true)][string]$ServerRoot,
    [string]$BuildDir = 'build'
)
$ErrorActionPreference = 'Stop'
$automationRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$server = (Resolve-Path $ServerRoot).Path
$binary = Join-Path $server "$BuildDir/bin/Release/MetasequoiaImeServer.exe"
$contracts = Join-Path $server 'MetasequoiaImeEngine/contracts'
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
$process = Start-Process -FilePath $binary -ArgumentList '--watchdog-managed' -WorkingDirectory (Split-Path $binary) -PassThru
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

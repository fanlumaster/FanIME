# Configure and build the Server; do not reuse old binaries after a failed build.
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build-release'
Push-Location $projectRoot
try {
    cmake --preset=vcpkg-release
    if ($LASTEXITCODE -ne 0) { throw "Server configure failed ($LASTEXITCODE)" }
    cmake --build $buildDirectory --config Release
    if ($LASTEXITCODE -ne 0) { throw "Server build failed ($LASTEXITCODE)" }
    # Keep the local SDK selection; the complete resource argument must reach mt.exe as one value.
    $binary = Join-Path $buildDirectory 'bin/Release/MetasequoiaImeServer.exe'
    & 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\mt.exe' `
        -manifest (Join-Path $projectRoot 'MetasequoiaImeServer.manifest') "-outputresource:$binary;1"
    if ($LASTEXITCODE -ne 0) { throw "Server manifest embedding failed ($LASTEXITCODE)" }
} finally { Pop-Location }

# Install the Boost packages MSIME-Server links but does not declare.
#
# Kept in step with metasequoiaime/MSIME-Server's own CI. Boost is linked statically but is not in
# its vcpkg.json, so it cannot come from the manifest. The triplet has to be static-md, not static:
# Boost_USE_STATIC_LIBS ON asks for static Boost libraries while the rest of the project builds
# against the dynamic CRT, and plain x64-windows-static would switch Boost to the static CRT and
# fail to link with LNK2038 RuntimeLibrary mismatches.
#
# Appends BOOST_ROOT to GITHUB_ENV, which is the override MSIME-Server's CMakeLists.txt supports.
$ErrorActionPreference = 'Stop'

Push-Location $env:VCPKG_INSTALLATION_ROOT
try {
    ./vcpkg install boost-locale:x64-windows-static-md boost-json:x64-windows-static-md
    if ($LASTEXITCODE -ne 0) { throw "vcpkg install failed with exit code $LASTEXITCODE" }
}
finally {
    Pop-Location
}

"BOOST_ROOT=$env:VCPKG_INSTALLATION_ROOT/installed/x64-windows-static-md" >> $env:GITHUB_ENV

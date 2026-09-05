#
# generate, compile and run exe files
#
function getExePathFromCMakeLists([string]$buildDir, [string]$config) {
    $content = Get-Content -Raw -Path "./CMakeLists.txt"
    $exePath = ""
    foreach ($line in $content -split "`n") {
        if ($line -match 'set\(MY_EXECUTABLE_NAME[^\"]*\"([^\"]+)\"') {
            $exeName = $matches[1]
            $exePath = Join-Path -Path $buildDir -ChildPath ("bin/{0}/{1}.exe" -f $config, $exeName)
            break
        }
    }
    return $exePath
}

$currentDirectory = Get-Location
$cmakeListsPath = Join-Path -Path $currentDirectory -ChildPath "CMakeLists.txt"

if (-not (Test-Path $cmakeListsPath)) {
    Write-Host("No CMakeLists.txt in current directory, please check.")
    return
}

Write-Host "Start generating and compiling..."

$buildFolderPath = ".\build-release"
$presetName = "vcpkg-release"
$buildConfig = "Release"

if (-not (Test-Path $buildFolderPath)) {
    New-Item -ItemType Directory -Path $buildFolderPath | Out-Null
    Write-Host "build-release folder created."
}

# cmake -G "Visual Studio 18 2026" -A x64 -S . -B ./build-release
cmake --preset=$presetName

if ($LASTEXITCODE -eq 0) {
    cmake --build $buildFolderPath --config $buildConfig
    if ($LASTEXITCODE -eq 0) {
        $exePath = getExePathFromCMakeLists -buildDir $buildFolderPath -config $buildConfig
        Write-Host "start running as follows..."
        Write-Host "=================================================="
        Invoke-Expression $exePath
    }
}

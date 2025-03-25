#
# run exe file that has already been compiled before
#
function getExePathFromCMakeLists() {
    $content = Get-Content -Raw -Path "./CMakeLists.txt"
    $exePath = ""
    foreach ($line in $content -split "`n") {
        if ($line -match 'set\(MY_EXECUTABLE_NAME[^\"]*\"([^\"]+)\"') {
            $exeName = $matches[1]
            $exePath = "./build/bin/Debug/$exeName" + ".exe"
            break
        }
    }
    return $exePath
}

$exePath = getExePathFromCMakeLists
#Write-Host "start running as follows..."
#Write-Host "=================================================="
Invoke-Expression $exePath
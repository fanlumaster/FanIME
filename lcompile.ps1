# generate and compile exe files
# 默认是编译 64 位的 dll，如果指定 32 位，则编译 32 位 dll

$currentDirectory = Get-Location
$cmakeListsPath = Join-Path -Path $currentDirectory -ChildPath "CMakeLists.txt"

if (-not (Test-Path $cmakeListsPath))
{
    Write-Host("No CMakeLists.txt in current directory, please check.")
    return
}

Write-Host "Start generating and compiling..."


$arch = $args[0]

if ($arch -eq "32")
{
    $buildFolderPath = ".\build32"

    if (-not (Test-Path $buildFolderPath))
    {
        New-Item -ItemType Directory -Path $buildFolderPath | Out-Null
        Write-Host "build32 folder created."
    }
    cmake -G "Visual Studio 17 2022" -A Win32 -S . -B ./build32/

    if ($LASTEXITCODE -eq 0)
    {
        cmake --build ./build32/ --config DEBUG
    }
} elseif ($arch -eq "64")
{
    $buildFolderPath = ".\build64"

    if (-not (Test-Path $buildFolderPath))
    {
        New-Item -ItemType Directory -Path $buildFolderPath | Out-Null
        Write-Host "build64 folder created."
    }
    cmake -G "Visual Studio 17 2022" -A x64 -S . -B ./build64/

    if ($LASTEXITCODE -eq 0)
    {
        cmake --build ./build64/ --config DEBUG
    }
} else
{ # 默认是 64 位
    $buildFolderPath = ".\build64"

    if (-not (Test-Path $buildFolderPath))
    {
        New-Item -ItemType Directory -Path $buildFolderPath | Out-Null
        Write-Host "build64 folder created."
    }
    cmake -G "Visual Studio 17 2022" -A x64 -S . -B ./build64/

    if ($LASTEXITCODE -eq 0)
    {
        cmake --build ./build64/ --config DEBUG
    }
}


#
# generate, compile and run exe files
#

function getExePathFromCMakeLists()
{
  $content = Get-Content -Path "./CMakeLists.txt"
  $lastLine = ""
  $contentContainedExeName = ""
  foreach($line in $content)
  {
    if ($line.StartsWith("add_executable"))
    {
      $index = $line.IndexOf("(")
      $contentContainedExeName = $line.Substring($index + 1)
      if ([string]::IsNullOrEmpty($contentContainedExeName))
      {
        $lastLine = $line
        continue
      }
      break
    } elseif ($lastLine.StartsWith("add_executable"))
    {
      $contentContainedExeName = $line
      break
    }
    $lastLine = $line
  }
  $result = -split $contentContainedExeName
  $exePath = "./build/DEBUG/" + $result[0] + ".exe"
  return $exePath
}

$currentDirectory = Get-Location
$cmakeListsPath = Join-Path -Path $currentDirectory -ChildPath "CMakeLists.txt"

if (-not (Test-Path $cmakeListsPath))
{
  Write-Host("No CMakeLists.txt in current directory, please check.")
  return
}

Write-Host "Start generating and compiling..."

$buildFolderPath = ".\build"

if (-not (Test-Path $buildFolderPath))
{
  New-Item -ItemType Directory -Path $buildFolderPath | Out-Null
  Write-Host "build folder created."
}

cmake -G "Visual Studio 17 2022" -A x64 -S . -B ./build/

if ($LASTEXITCODE -eq 0)
{
  cmake --build ./build/ --config DEBUG
  if ($LASTEXITCODE -eq 0)
  {
    $exePath = getExePathFromCMakeLists
    Write-Host "start running as follows..."
    Write-Host "=================================================="
    Invoke-Expression $exePath
  }
}

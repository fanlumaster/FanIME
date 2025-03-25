#
# run exe file that has already been compiled before
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

$exePath = getExePathFromCMakeLists
Write-Host "start running as follows..."
Write-Host "=================================================="
Invoke-Expression $exePath

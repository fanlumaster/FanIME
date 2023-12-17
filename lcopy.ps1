Get-ChildItem "C:\Program Files\SampleIME\"
Write-Host "=============================="
Copy-Item ./build64/DEBUG/SampleIME.dll "C:\Program Files\SampleIME\SampleIME.dll"
Get-ChildItem "C:\Program Files\SampleIME\"

# Give the compiled installer its final name, record its checksum and summarise the build.
#
# Inno Setup names the output from MyAppVersion alone; unsigned builds get a suffix here so the
# published asset says what it is.
#
# Requires TARGET_VERSION, ASSET_SUFFIX, DICTIONARY_TAG. Run from the installer directory.
# Writes asset_path, asset_name and asset_sha256 to GITHUB_OUTPUT.
$ErrorActionPreference = 'Stop'

$built = "Output/MetasequoiaIME_Setup_v$env:TARGET_VERSION.exe"
if (-not (Test-Path $built)) { throw "Inno Setup did not produce $built" }

$final = "Output/MetasequoiaIME_Setup_v$env:TARGET_VERSION$env:ASSET_SUFFIX.exe"
if ($built -ne $final) { Move-Item $built $final -Force }

$hash = (Get-FileHash $final -Algorithm SHA256).Hash.ToLower()
$size = [math]::Round((Get-Item $final).Length / 1MB, 1)
$name = Split-Path $final -Leaf

"asset_path=$((Resolve-Path $final).Path)" >> $env:GITHUB_OUTPUT
"asset_name=$name" >> $env:GITHUB_OUTPUT
"asset_sha256=$hash" >> $env:GITHUB_OUTPUT

@(
    '### Installer'
    ''
    '| Field | Value |'
    '| --- | --- |'
    "| File | $name |"
    "| Size | $size MB |"
    "| SHA256 | ``$hash`` |"
    "| Dictionaries | $env:DICTIONARY_TAG |"
) | Add-Content -Path $env:GITHUB_STEP_SUMMARY

Write-Host "$name ($size MB, sha256 $hash)"

# Confirm the build produced the DLL the installer collects.
#
# Reading a version back off the DLL would not work: the VERSIONINFO block in
# src/IME/MetasequoiaIME.rc is named IDR_VERSION2, which is not defined anywhere, rather than the
# VS_VERSION_INFO the Windows version API looks for, so GetFileVersionInfo finds nothing and the
# DLL reports an empty FileVersion. apply_version.py --check covers the version instead.
param([Parameter(Mandatory)][string]$BuildDir)

$ErrorActionPreference = 'Stop'

$dll = Join-Path $BuildDir 'Release/MetasequoiaImeTsf.dll'
if (-not (Test-Path $dll)) { throw "The build did not produce $dll" }
Write-Host "$dll is $((Get-Item $dll).Length) bytes"

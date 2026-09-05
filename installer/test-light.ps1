# 轻量本地测试：只编译 TSF / Server / 设置页，收集这三块产物，打不含词库的安装包。
#
# 适用：只改了 TSF、Server 或 HTML，本机已有完整安装（词库已在
# %LOCALAPPDATA%\metasequoiaime）。不要用这个脚本做首次安装。
#
# 完整流程（含词库）仍用 .\test.ps1。

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Set-Location $PSScriptRoot

$repoRoot = Split-Path -Parent $PSScriptRoot
$tsfCompile = Join-Path $repoRoot 'MetasequoiaImeTsf\scripts\lcompile-release-both.ps1'
$serverCompile = Join-Path $repoRoot 'MetasequoiaImeServer\scripts\lcompile-release.ps1'
$settingsDir = Join-Path $repoRoot 'MetasequoiaImeUiHtml\webview2\settings\ime-settings'

if (Test-Path -LiteralPath $tsfCompile -PathType Leaf) {
    Push-Location (Join-Path $repoRoot 'MetasequoiaImeTsf')
    try { & $tsfCompile } finally { Pop-Location }
}
else {
    Write-Host '跳过 TSF 编译：未找到相邻的 MetasequoiaImeTsf 仓库。'
}

if (Test-Path -LiteralPath $serverCompile -PathType Leaf) {
    Push-Location (Join-Path $repoRoot 'MetasequoiaImeServer')
    try { & $serverCompile } finally { Pop-Location }
}
else {
    Write-Host '跳过 Server 编译：未找到相邻的 MetasequoiaImeServer 仓库。'
}

if (Test-Path -LiteralPath (Join-Path $settingsDir 'package.json') -PathType Leaf) {
    Push-Location $settingsDir
    try { pnpm run build } finally { Pop-Location }
}
else {
    Write-Host '跳过设置页构建：未找到相邻的 MetasequoiaImeUiHtml 仓库。'
}

& (Join-Path $PSScriptRoot 'Prepare-PackageFiles.ps1') -Light
& (Join-Path $PSScriptRoot 'Sign-PackageBinaries-Local.ps1')
& (Join-Path $PSScriptRoot 'Compile-Installer.ps1') -Light
& (Join-Path $PSScriptRoot 'Sign-Installer-Local.ps1') -Light
& (Join-Path $PSScriptRoot 'Install.ps1') -Light

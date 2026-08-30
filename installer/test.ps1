# 本地测试安装流程：编译相邻源码仓库（若存在）→ 收集安装文件 →
# 本机自签名 → Inno Setup 打包 → 签安装包 → 启动安装程序。
#
# 不使用任何预置证书。签名脚本会在本机生成并复用自签名测试证书。

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

& (Join-Path $PSScriptRoot 'Prepare-PackageFiles.ps1')
& (Join-Path $PSScriptRoot 'Sign-PackageBinaries-Local.ps1')
& (Join-Path $PSScriptRoot 'Compile-Installer.ps1')
& (Join-Path $PSScriptRoot 'Sign-Installer-Local.ps1')
& (Join-Path $PSScriptRoot 'Install.ps1')

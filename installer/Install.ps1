# 启动本目录 Output\ 里刚编译好的安装包。
# 安装包文件名从 msime_setup.iss 的 MyAppVersion 读出。

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$issPath = Join-Path $PSScriptRoot 'msime_setup.iss'
if (-not (Test-Path -LiteralPath $issPath -PathType Leaf)) {
    throw "找不到安装脚本：$issPath"
}
$issContent = Get-Content -LiteralPath $issPath -Raw
if ($issContent -notmatch '(?m)^#define\s+MyAppVersion\s+"(?<version>[^"]+)"') {
    throw '未能在 msime_setup.iss 中找到 MyAppVersion。'
}

$installerPath = Join-Path $PSScriptRoot "Output\MetasequoiaIME_Setup_v$($Matches.version).exe"
if (-not (Test-Path -LiteralPath $installerPath -PathType Leaf)) {
    throw "安装文件不存在，请先运行 Compile-Installer.ps1：$installerPath"
}

& $installerPath

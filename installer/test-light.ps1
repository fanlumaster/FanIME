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
$tsfCompile = Join-Path $repoRoot 'windows\scripts\lcompile-release-both.ps1'
$serverCompile = Join-Path $repoRoot 'server\scripts\lcompile-release.ps1'
$settingsDir = Join-Path $repoRoot 'ui-html\webview2\settings\ime-settings'

# All three are directories of this repository now, so a missing one is a broken checkout rather
# than a neighbour nobody cloned. Skipping the build and packaging whatever binaries happen to be
# lying around would produce an installer you then run on your own machine.
foreach ($required in @($tsfCompile, $serverCompile, (Join-Path $settingsDir 'package.json'))) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "缺少组件构建入口：$required"
    }
}

Push-Location (Join-Path $repoRoot 'windows')
try { & $tsfCompile } finally { Pop-Location }

Push-Location (Join-Path $repoRoot 'server')
try { & $serverCompile } finally { Pop-Location }

Push-Location $settingsDir
try { pnpm run build } finally { Pop-Location }

& (Join-Path $PSScriptRoot 'Prepare-PackageFiles.ps1') -Light -RepoRoot $repoRoot -TsfDirectory windows -ServerDirectory server -UiHtmlDirectory ui-html -NoticesDirectory .
& (Join-Path $PSScriptRoot 'Sign-PackageBinaries-Local.ps1')
& (Join-Path $PSScriptRoot 'Compile-Installer.ps1') -Light
& (Join-Path $PSScriptRoot 'Sign-Installer-Local.ps1') -Light
& (Join-Path $PSScriptRoot 'Install.ps1') -Light

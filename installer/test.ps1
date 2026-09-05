# 本地测试安装流程：编译本仓组件→ 收集安装文件 →
# 本机自签名 → Inno Setup 打包 → 签安装包 → 启动安装程序。
#
# 不使用任何预置证书。签名脚本会在本机生成并复用自签名测试证书。
# 只改 TSF / Server / HTML、且本机已有词库时，用 .\test-light.ps1。

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
try {
    pnpm run build
    if ($LASTEXITCODE -ne 0) { throw "Settings page build failed ($LASTEXITCODE)" }
} finally { Pop-Location }

& (Join-Path $PSScriptRoot 'Prepare-PackageFiles.ps1') -RepoRoot $repoRoot -TsfDirectory windows -ServerDirectory server -UiHtmlDirectory ui-html -NoticesDirectory .
& (Join-Path $PSScriptRoot 'Sign-PackageBinaries-Local.ps1')
& (Join-Path $PSScriptRoot 'Compile-Installer.ps1')
& (Join-Path $PSScriptRoot 'Sign-Installer-Local.ps1')
& (Join-Path $PSScriptRoot 'Install.ps1')

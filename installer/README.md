# Metasequoia IME Installer（本地测试流程）

公开这份仓库的目的，是把 Metasequoia IME 的 **本地测试安装流程** 整理清楚：从相邻源码仓库收集产物、用本机自签名证书签名、用 Inno Setup 打安装包、再跑安装程序。

测试版本号固定为 **0.0.1**。

本仓库 **不包含** 正式代码签名证书、私钥、指纹，也不包含编译好的 EXE/DLL、词库或安装包。签名脚本会在你自己的机器上生成并复用一张自签名测试证书。

## 依赖

- Windows 10/11，PowerShell 7（`pwsh`）
- [Inno Setup](https://jrsoftware.org/isinfo.php) 6.6 或更高版本
- Windows SDK（提供 `signtool.exe`）
- 下列源码仓库需要你自己准备，并先完成 Release 编译 / 词库构建：
  - `MetasequoiaImeTsf`
  - `MetasequoiaImeServer`
  - `MetasequoiaImeUiHtml`
  - `MetasequoiaImeHelpCode`
  - `MetasequoiaImeDict`

## 请先改路径

脚本里写死的默认布局是：本仓库和上面那些项目都在**同一个父目录**下（例如都在 `IMECodes\` 里）。这只是作者本机的相对路径，**换到你自己的机器上通常对不上**。

请按自己的目录改这些地方，尤其是收集安装文件时的复制源：

- `Prepare-PackageFiles.ps1`：`$serverRelease`、`$tsf32Release`、`$tsf64Release`、`$webviewRoot`、`$helpcodeSource`、词库 `msime.db` / `english.db` 等，都是从其他项目里 **Copy** 过来的。路径不对就会直接报“不存在”。
- `test.ps1`：TSF / Server 的编译脚本、设置页的 `pnpm run build` 目录同样要改。

如果只是父目录不同、各子项目文件夹名没变，可以不改脚本，运行时传入：

```powershell
pwsh -File .\Prepare-PackageFiles.ps1 -RepoRoot "D:\your\src"
```

子项目不在同一父目录、或编译输出目录和默认不一致时，请直接改脚本里的那些路径。

## 一键测试

先确认上一节的路径已经改成你本机的。然后在管理员 PowerShell 中（把本机测试证书写入受信任存储需要提升权限）：

```powershell
cd path\to\msime-installer
pwsh -File .\test.ps1
```

`test.ps1` 会按顺序做这些事：

1. 若找到相邻仓库，则编译 TSF、Server，并构建设置页
2. `Prepare-PackageFiles.ps1` — 收集 `server_exe\`、`tsf_dll\`、`app_data\`，并把 `msime_setup.iss` 的版本写成 `0.0.1`
3. `Sign-PackageBinaries-Local.ps1` — 本机自签名包内 EXE/DLL
4. `Compile-Installer.ps1` — 编译出 `Output\MetasequoiaIME_Setup_v0.0.1.exe`
5. `Sign-Installer-Local.ps1` — 本机自签名安装包
6. `Install.ps1` — 启动安装程序

也可以按上面的顺序逐步运行。

## 本机测试证书

第一次运行签名脚本时，会在当前用户证书存储里创建：

`CN=Metasequoia IME Local Test Code Signing`

之后复用同一张证书。产物只适合本机或已导入该证书公钥的测试机，不要当作正式分发包。

想让另一台测试机信任这张证书，把公钥导出成 `.cer` 后在那台机器上执行：

```powershell
certutil -addstore Root MetasequoiaImeLocalTest.cer
certutil -addstore TrustedPublisher MetasequoiaImeLocalTest.cer
```

不要把 `.cer` / `.pfx` 提交进本仓库。

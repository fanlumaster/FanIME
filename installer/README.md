# Metasequoia IME Installer

本仓的脚本从相邻源码仓库收集产物、签名、用 Inno Setup 打成安装包。它服务两条流程：

- **正式发布**：由 `MSIME-Windows` 的 release workflow 驱动，用真实证书，产物是挂在 Release 上的 `MetasequoiaIME_Setup_v<版本>.exe`。见下面「CI 契约」
- **本地测试**：手工跑，用本机自签名证书，用来在自己机器上验证安装流程。本文其余部分讲的是这条

版本号不是固定的，由 `Prepare-PackageFiles.ps1` 的 `-TargetVersion` 决定，默认 `0.0.1`；正式发布时 CI 传入真实版本。

本仓库 **不包含** 正式代码签名证书、私钥、指纹，也不包含编译好的 EXE/DLL、词库或安装包。

签名脚本会在你自己的机器上生成并复用一张自签名测试证书。

## CI 契约

`MSIME-Windows` 的 release workflow **不加修改地调用本仓的 `Prepare-PackageFiles.ps1` 和 `Compile-Installer.ps1`**。它把各个仓库 checkout 到历史目录名下（`MetasequoiaImeTsf`、`MetasequoiaImeServer`、`MetasequoiaImeUiHtml`、`MetasequoiaImeHelpCode`、`MetasequoiaImeDict`），而这些名字现在是 `Prepare-PackageFiles.ps1` 的参数默认值，不再是写死在脚本里的字面量。

调用方如果用的是当前的仓库名，直接传参即可，不必为了迁就脚本去重命名目录：

```powershell
pwsh -File ./Prepare-PackageFiles.ps1 -TargetVersion 1.2.3 -RepoRoot ..\src `
    -TsfDirectory MSIME-Windows -ServerDirectory MSIME-Server `
    -UiHtmlDirectory MSIME-UiHtml -HelpCodeDirectory MSIME-HelpCode `
    -DictionaryDirectory MSIME-Dict
```

由此产生几条约束：

- **改动 `Prepare-PackageFiles.ps1` 里那些 `Assert-PathExists` 的源路径，会直接弄坏发布流水线。** 它断言的源路径由 workflow 逐一准备，改了要同步改 `MSIME-Windows/.github/workflows/release.yml`。目录名本身现在可以由调用方指定，但每个仓库内部的相对路径（如 `build-release\bin\Release`）仍是硬契约
- **`THIRD_PARTY_NOTICES.txt` 从 `-TsfDirectory` 指向的仓库根目录取，随安装包装到程序目录。** 词库主体含 rime-ice（GPL-3.0）内容，其许可要求保留署名，所以这份文件缺失会让打包直接失败，而不是静默跳过
- **`Sign-PackageBinaries-Local.ps1` 和 `Sign-Installer-Local.ps1` 只用于本地验证，CI 不会调用它们。** 它们创建本机自签名证书并尝试写入受信任存储，正式发布走 workflow 里用仓库 secret 中真实证书的签名步骤
- 词库不再从相邻的 `MetasequoiaImeDict` 工作目录取，CI 从 `MSIME-Dict` 的 `dict-*` release 下载并校验 SHA256，再放到脚本期望的位置
- `windows-2025` runner 自带 Inno Setup 6.7.1，`Compile-Installer.ps1` 能自己找到 `ISCC.exe`。但它不带 `ChineseSimplified.isl`，`msime_setup.iss` 的 `[Languages]` 段依赖那个文件，所以 workflow 会在编译前按固定 revision 和校验和把它装进去

## 依赖

- Windows 10/11，PowerShell 7（`pwsh`）
- [Inno Setup](https://jrsoftware.org/isinfo.php) 6.6 或更高版本
- Windows SDK（提供 `signtool.exe`）
- 下列源码仓库需要你自己准备，并先完成 Release 编译 / 词库构建：
  - `MetasequoiaImeTsf`（或用 `-TsfDirectory` 指定实际目录名，下同）
  - `MetasequoiaImeServer`
  - `MetasequoiaImeUiHtml`
  - `MetasequoiaImeHelpCode`
  - `MetasequoiaImeDict`

## 本地跑之前请先改路径

下面这段只针对本地测试；CI 不需要改路径，因为它就是按这些名字准备目录的。

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

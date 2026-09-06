# 参与 MSIME-Windows 开发

组织级的通用约定（行为准则、PR 要求、安全上报）见[组织贡献指南](https://github.com/metasequoiaime/.github/blob/main/CONTRIBUTING.md)。这份文档只讲本仓特有的东西：环境、怎么把它编出来、改哪一层、以及提交前要跑什么。

想找活干的话先看[招募开源开发者](https://github.com/metasequoiaime/.github/blob/main/RECRUITING.md)和 [good first issue](https://github.com/metasequoiaime/MSIME-Windows/issues?q=is%3Aissue+is%3Aopen+label%3A%22good+first+issue%22)。不写代码也有事情可做：词库条目、文档、图标资源、兼容性测试都在 [no-code](https://github.com/metasequoiaime/MSIME-Windows/issues?q=is%3Aissue+is%3Aopen+label%3Ano-code) 标签下。

## 环境

- Windows 10 / 11
- Visual Studio 2026（需要 C++ 桌面开发工作负载）
- CMake 3.25+
- vcpkg，并把 `VCPKG_ROOT` 设进环境变量
- Python 3.10+
- Inno Setup 6.6+（只有要打安装包时才需要）

Boost 按 `-DBoost_ROOT=...` → 环境变量 `BOOST_ROOT` → `%USERPROFILE%\scoop\apps\boost\current` 的顺序解析。preset 里的 vcpkg 路径取自 `$env{VCPKG_ROOT}`，不需要改文件。

## 取代码

```powershell
git clone --recursive https://github.com/metasequoiaime/MSIME-Windows.git
cd MSIME-Windows
```

忘了 `--recursive` 的话补一句 `git submodule update --init --recursive`。`vendor/MetasequoiaImeEngine` 是跨平台输入引擎，缺了它任何一个 C++ 组件都编不过。

词库不在版本库里，构建时从 MSIME-Engine 的 `dict-*` release 按锁定的 tag 和摘要取用：

```powershell
python scripts/product_lock.py fetch-dictionaries --staging-root .
```

## 构建

没有顶层聚合的 `CMakeLists.txt`，这是有意的：TSF DLL 用静态 CRT、Server 用动态 CRT，两者的 vcpkg manifest 与编译选项互不兼容，合成一个 build tree 会互相污染。每个组件是独立的 CMake 工程，从仓库根目录指定 `-S`：

```powershell
cmake -S windows -B windows/build64 -A x64        # TSF 文本服务 DLL
cmake -S server  -B server/build    -A x64        # Server
cmake -S ui      -B ui/build        -A x64        # 自研 GUI 框架 msimeui
```

`server/` 通过 `add_subdirectory(../ui ...)` 把 `msimeui` 拉进自己的构建，这是唯一的跨组件构建引用。

每个组件也各自带了本地脚本，比手敲 cmake 省事：

```powershell
.\windows\scripts\prepare_env.py    # 生成 .clangd 并把 preset 指向本机 vcpkg/Boost
.\windows\scripts\lcompile.ps1 64   # Debug；换 32 编 32 位
.\server\scripts\lcompile.ps1
```

各组件更细的说明在各自的 README 与 AGENTS.md 里，那两份文件写给人和 AI 都一样有效：

- [`windows/AGENTS.md`](windows/AGENTS.md) — TSF 层的构建、注册与**手工回归清单**（这份清单是本仓最难替代的知识，改 TSF 前先读）
- [`server/README.md`](server/README.md) — Server 的开发构建与本地词库准备
- [`ui/README.md`](ui/README.md) — GUI 框架的构建与边界约束
- [`installer/README.md`](installer/README.md) — 打包
- [`AGENTS.md`](AGENTS.md) — 跨组件契约：协议只有一份来源，改哪边要同时改哪边

## 打一个可安装的包

从仓库根开始，严格按顺序。任一步失败先修，不要拿旧产物继续打包：

```powershell
Set-Location .\windows
.\scripts\lcompile-release-both.ps1

Set-Location ..\server
.\scripts\lcompile-release.ps1

Set-Location ..\installer
.\Prepare-PackageFiles.ps1 -RepoRoot .. -TsfDirectory windows -ServerDirectory server -UiHtmlDirectory ui-html -NoticesDirectory .
.\Sign-PackageBinaries-Local.ps1
.\Compile-Installer.ps1
```

`Sign-PackageBinaries-Local.ps1` 用的是本机测试证书，只用于内部安装验证；不要把本机证书指纹或私钥提交进仓库。

## 改动前要知道的几条边界

- **协议的唯一来源是 Engine 的 `contracts/`。** IPC 线格式、opcode、语音分帧和 WebView 消息定义都在 `vendor/MetasequoiaImeEngine/contracts/`，两侧引用同一份头文件，不要在任何一侧另写一份。
- **`ui/` 不许反向依赖产品。** 它不读 Server 配置、IPC、引擎、词库或全局输入状态。`ui/scripts/check-boundary.py` 在 CI 里检查。
- **窗口归 `server/`，页面归 `ui-html/`。** 改消息 `type`、JSON 字段或页面导出的 JS 函数时两侧一起改。
- **`ui-html/webview2/shared/` 是生成物**，由 `ui-html/scripts/sync-contracts.py` 从 submodule 同步，CI 用 `--check` 校验。要改契约就去改 Engine。
- **候选与输入状态的权威在 Server 和引擎**，页面只负责展示。

## 提交前

```powershell
ctest --test-dir server/build --output-on-failure       # Server 单元测试
python -m unittest discover -s tests                    # 产品级组合验证
bash scripts/ci/check-developer-paths.sh                # 构建输入里不许出现个人目录路径
```

改了 TSF 层就照 [`windows/AGENTS.md`](windows/AGENTS.md#构建与验证) 的手工回归清单跑一遍。输入法很难靠读代码判断对错，**PR 里必须写清你怎么验证的**：系统版本、宿主程序、操作步骤、观察到的结果、跑过的测试命令。

`git status --short` 确认只暂存本次任务涉及的路径，不要 `git add -A`。提交信息用 `type(scope): 摘要`，不要加 `Co-Authored-By` 或任何 AI 生成标记。

分支从 `develop` 切，PR 也合回 `develop`——它是本仓默认分支，开 PR 时不用改 base。`main` 是发布分支，只在发版时由维护者从 `develop` 合入；特性分支直接提到 `main` 会被 `Branch guard` 检查拦下，把 base 改回 `develop` 即可。发布链条见 [docs/product-release.md](docs/product-release.md)。

不要在 PR、Issue、截图或日志里泄露 API Key。仓库开了 secret scanning 和 push protection，但那是最后一道防线不是第一道。

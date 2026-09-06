# AGENTS.md — 水杉输入法 Windows 端

组织级边界与跨平台规则见 [组织 AGENTS.md](https://github.com/metasequoiaime/.github/blob/main/AGENTS.md)。本文件规定本仓的目录职责、组件之间的边界，以及产品级的构建与发布约定。

本仓默认分支是 `develop`，日常改动从 `develop` 切分支并合回 `develop`；`main` 是发布分支，只在发版时由维护者从 `develop` 合入，`release.yml` 也只监听 `main`。特性分支直接提到 `main` 会被 `Branch guard` 拦下。规则见[组织 AGENTS.md 的分支模型](https://github.com/metasequoiaime/.github/blob/main/AGENTS.md#分支模型)。

Windows 端的全部一方源码在本仓。**合仓改变的是仓库数量，不是运行时结构**：DLL 与 Server 仍是两个进程、隔着版本化的命名管道；`ui/` 仍是不依赖输入法业务的通用 GUI 库。不要因为它们现在在同一棵树里，就在组件之间直接互相 include 或共享全局状态。

## 仓库地图

| 目录 | 职责 | 本目录的规则 |
|---|---|---|
| `windows/` | TSF 文本服务 DLL：按键预判、焦点、edit session、管道客户端 | [windows/AGENTS.md](windows/AGENTS.md) |
| `server/` | 常驻后端：候选状态、配置、词库、管道服务、窗口与 WebView2 宿主 | [server/AGENTS.md](server/AGENTS.md) |
| `ui/` | 通用 Win32 / Direct2D / DirectWrite 控件库 `msimeui` | [ui/AGENTS.md](ui/AGENTS.md) |
| `ui-html/` | 候选窗、工具栏、菜单、设置页的 HTML / CSS / JS | [ui-html/README.md](ui-html/README.md) |
| `installer/` | 收集产物、自签名、Inno Setup 打包 | [installer/README.md](installer/README.md) |
| `log/` | 日志采集库 | — |
| `experiments/tsf-edit-control/` | TSF 编辑控件实验工程，不参与产品构建 | — |
| `vendor/` | submodule：`MetasequoiaImeEngine`、`opencc`、`cpp-pinyin` | 上游仓库 |
| `scripts/`、`tests/`、`docs/` | 产品级构建与发布脚本、组合验证、产品文档 | 本文件 |

改一个组件之前先确认它有没有自己的 AGENTS.md，那份比本文件更具体。

## 组件之间的边界

- **协议的唯一来源是 Engine 的 `contracts/`**。IPC 线格式、opcode、语音分帧和 WebView 消息定义都在 `vendor/MetasequoiaImeEngine/contracts/`，`windows/` 和 `server/` 各自引用同一份头文件。不要在任何一侧重新定义或复制一份。
- **`ui/` 不许反向依赖产品**。它不读 Server 配置、IPC、引擎、词库或全局输入状态；业务通过数据和回调接入。`ui/scripts/check-boundary.py` 在 CI 里检查已知的反向依赖，新增依赖会红。
- **窗口归 `server/`，页面归 `ui-html/`**。HWND、尺寸、位置、DPI、Z-order 和 WebView2 controller 的生命周期在 `server/src/window/` 与 `server/src/webview2/`；页面结构、样式和浏览器端交互在 `ui-html/webview2/`。改消息 `type`、JSON 字段或页面导出的 JS 函数时两侧要一起改。
- **候选与输入状态的权威在 Server 和引擎**，页面只负责展示和发出用户动作，不要在网页侧复制状态机。
- `ui-html/webview2/shared/` 是 Engine web 契约的副本，由 `ui-html/scripts/sync-contracts.py` 生成，CI 用 `--check` 验证它和 submodule 一致。手改这个目录会被 CI 拦下来，改契约要改 Engine。

Server 当前仍使用 Engine 的兼容 `InputSession`，尚未迁移公共 `Session` facade。
辅助码筛选与候选提示都按会话配置，禁止用全局默认码表驱动活动会话。
`RuntimePaths::legacy()` 在适配器创建时捕获现有安装布局；完整资源包和用户数据代际切换仍待接入。

## 构建

每个组件是独立的 CMake 工程，各自带 `vcpkg.json` 和 preset，从仓库根目录指定 `-S`：

```powershell
cmake -S windows -B windows/build -A x64 ...      # TSF DLL
cmake -S server  -B server/build-release -A x64 ... # Server
cmake -S ui      -B ui/build -A x64                # GUI 框架
```

没有顶层聚合的 `CMakeLists.txt`，这是有意的：Windows tip 用静态 CRT、Server 用动态 CRT，两者的 vcpkg manifest 和编译选项互不兼容，合成一个 build tree 会互相污染。`server/` 通过 `add_subdirectory(../ui ...)` 把 `msimeui` 拉进自己的构建，这是唯一的跨组件构建引用。

`vendor/` 是 submodule，先 `git submodule update --init --recursive`。

## 产品输入清单

`product-lock.json` 只记录仍来自仓外的东西：Engine（在本仓是 submodule）、词库 Release 的 tag、source commit 和每个产物的 SHA256。**Server、页面、GUI 框架和安装器不在清单里**——它们是本仓的目录，本仓的一个 commit 就已经把它们钉住了。辅助码也不在——它已经并入 Engine，钉住契约的那个 gitlink 同时钉住了辅助码表。

- 引擎的权威是 `vendor/MetasequoiaImeEngine` 的 gitlink；`product-lock.json` 里的 `engine.commit` 只是把它记下来，供产物清单和发布门禁使用。两者必须一致，`product_lock.py verify-contracts` 会检查。bump submodule 时同一个 PR 里把 `engine.commit` 改过来。
- `refresh` 不再解析任何浮动源码引用：引擎取自本地 gitlink，词库取自指定 tag。词库清单里记录了构建它的 commit 和当时工作树是否干净，`verify-assets` 一并校验——摘要只能证明字节是评审过的字节，证明不了它来自一个能重建的源。
- 发布门禁 `verify-published` 要求清单里每个提交都能从各自仓库默认分支到达，只在发布路径执行，不进 CI。理由见 [docs/product-release.md](docs/product-release.md)。

## 正式发布（CI）

本地测试打包见 [windows/AGENTS.md](windows/AGENTS.md) 的构建与验证。对外发布走 `.github/workflows/release.yml`，产出的就是历来挂在本仓 Release 上的 `MetasequoiaIME_Setup_v<版本>.exe`。

版本号由 release-please 管理，真源是根目录的 `version.txt`。**从 `main` 往后是全自动的**：把 `develop` 合进 `main`，只要这批提交里有 `fix:` 或 `feat:`，最后就会有一个签好名的安装包挂在 Release 上，中间没有任何一步等人。发版的决定点因此是那次提升本身——合进 `develop` 不产生版本号，也不消耗签名额度。

这一整条链跑在那次 push 触发的同一个 run 里：

1. release-please 先用 `RELEASE_PLEASE_TOKEN` 补建已有合并版本的元数据，再用该凭据把这次 push 的提交刷进 release PR，触发正常的 PR 检查。
2. `land-release-pr.sh` 等待该 PR 精确 head 的 `pull_request` CI，绿了就用 `GITHUB_TOKEN` 合并它。不能以手动派发检查替代 PR 检查：后者才会进入 PR 的检查汇总并满足门禁。
3. release-please 再跑一次，用 `RELEASE_PLEASE_TOKEN` 只创建 draft release 和 tag（`skip-github-pull-request: true`）。维护 PR 的调用只写版本分支（`skip-github-release: true`）。
4. 构建、签名、把 exe 挂上去、draft 转正式。

合并用的是 `GITHUB_TOKEN`，而用该 token 推的提交不会触发 workflow —— 这是设计依赖的性质，不是要绕开的限制：它保证一次 push 只有一个 Release run，不会再冒出第二个卡在 concurrency 上。手工点 merge 那个 PR 也能得到同样结果，只是提前了一个 run。

**每次可发布的合并都花掉一次签名额度**，这正是之前的手动设计要避免的事。取舍的理由记在 [docs/product-release.md](docs/product-release.md)。

`workflow_dispatch` 保留下来，它同时是修复通道和发布频道的入口：run 在建出 draft 之后失败时，拿那个 tag 重跑即可。同一个 tag 重复触发会被 `validate-draft-release.sh` 挡下——发布之后它就不是 draft 了，不会重复发布同一个版本。

**两个频道由触发方式决定，`publish-release.sh` 据此选状态**（[#167](https://github.com/metasequoiaime/MSIME-Windows/issues/167)）：

| 频道 | 触发 | 发布状态 | 标题 |
|---|---|---|---|
| 自动构建 | push | `--prerelease` | `v<版本>（自动构建）`，说明里带提示 |
| 发布 | `workflow_dispatch` | `--prerelease=false --latest` | `v<版本>` |

GitHub 没有自定义频道，只有 Latest / Pre-release / Draft 三种状态，所以这里用 `prerelease` 标志承载「自动、未经挑选」，而不是承载「内测」。产品仍在内测这件事写在 release 说明和 [docs/installation.md](docs/installation.md) 里——那才是该做稳定性声明的地方，而这个标志同时还得用来分隔频道，兼不了两份职责。

`windows/src/IME/MetasequoiaIME.rc` 的 `FILEVERSION` / `PRODUCTVERSION` 不由 release-please 直接改（它是逗号和点号两种写法），而是由 `scripts/apply_version.py` 从 `version.txt` 注入，release 构建在 configure 之前执行：

```powershell
python .\scripts\apply_version.py           # 从 version.txt 写入版本资源
python .\scripts\apply_version.py --check   # 只检查是否与 version.txt 一致
```

（`scripts/apply_version.py` 在仓根，改的是 `windows/` 下的版本资源。）

release workflow 里的每一段 shell 都抽在 `scripts/ci/` 下，workflow 本身只负责编排和传参：

| 脚本 | 用途 |
|---|---|
| `validate-draft-release.sh` | 手动触发时校验 tag 是未发布 draft、指向不可变 commit、且与 `version.txt` 一致 |
| `land-release-pr.sh` | 给 release PR 挂上一次通过的 CI，绿了就合并，并等合并提交在 `main` 上可见 |
| `resolve-auto-release.sh` | 从两次 release-please 调用里选出本次要构建的 release，并做和手动路径同样的不可变性校验 |
| `verify-commit-on-main.sh` | 拒绝构建不在 main 历史里的 commit |
| `install-boost.ps1` | 装 Server 链接但未声明的 Boost，triplet 必须是 static-md |
| `check-server-binaries.ps1` | 提前拦住 Server 产物缺文件 |
| `check-tsf-dll.ps1` | 确认 TSF DLL 产出 |
| `download-dictionaries.sh` | 从产品锁指定仓库的 `dict-*` release 拉词库并校验 SHA256 |
| `detect-release-signing.ps1` | 判定签名模式，决定产物后缀 |
| `sign-binaries.ps1` | 用仓库 secret 里的真证书签名，包内二进制和安装包共用 |
| `install-inno-language.ps1` | 补 runner 上缺失的 `ChineseSimplified.isl`，按 commit + SHA256 固定。装到真正的 Inno Setup 安装目录，不是 Chocolatey shim 旁边 |
| `check-inno-language.ps1` | CI 用：编译一个只含 `[Languages]` 的探针脚本，让 ISCC 自己回答语言文件放对没有 |
| `name-installer-asset.ps1` | 定最终产物名、算校验和、写 step summary |
| `revalidate-draft-release.sh` | 发布前复查 draft 仍指向被构建的那个 commit |
| `publish-release.sh` | 上传产物、追加说明、发布 |

合仓之前 CI 要把五个仓 checkout 到历史目录名下，`Prepare-PackageFiles.ps1` 才能不改一行地跑。现在源目录名由 workflow 显式传参（`-TsfDirectory windows -ServerDirectory server -UiHtmlDirectory ui-html -NoticesDirectory . -HelpCodeDirectory vendor/MetasequoiaImeEngine/helpcode`），只有词库还落在仓根的 `MetasequoiaImeDict/`，用的是那个参数的默认值。改动这些脚本里的产物路径时，要连同 release workflow 一起核对。

词库不在 CI 里现建，从产品锁指定仓库的 `dict-*` release 下载并校验 SHA256。词库源数据与构建入口已并入 MSIME-Engine，现有 MSIME-Dict release 作为不可变旧产物保留；换发布源要在 `product_lock.py` 里明确评审，不是改个 tag 就能悄悄完成的事。词库改了要先在 Engine 跑构建 workflow 并勾选 publish，再发 Windows 版本；通过 `scripts/product_lock.py refresh --dictionary-tag <tag>` 更新产品锁并评审摘要变更；发布构建不能临时覆盖词库版本。

签名沿用「有证书就签、没有就发未签名版」的策略：配置了 `WINDOWS_SIGNING_CERTIFICATE_BASE64` 和 `WINDOWS_SIGNING_CERTIFICATE_PASSWORD` 两个 secret 时，用 `signtool` 签包内 EXE/DLL 和安装包；没配置时产物名带 `-unsigned` 后缀，并在 release 说明里写明 `uiAccess` 不会生效。`Sign-PackageBinaries-Local.ps1` 使用本机自签名证书，只用于本地验证，CI 不会调用它。

## 提交

提交信息用 `type(scope): 摘要`，scope 用目录名（`windows`、`server`、`ui`、`ui-html`、`installer`）。不要添加 `Co-Authored-By`、`Generated with` 或其他 AI 生成标记。

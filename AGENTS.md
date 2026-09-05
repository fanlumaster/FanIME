# AGENTS.md — MSIME-Windows

组织级边界与跨仓规则见 [组织 AGENTS.md](https://github.com/metasequoiaime/.github/blob/main/AGENTS.md)。本文件只规定 Windows TSF 前端、COM/焦点/线程、注册和产品构建。

## 本仓地图

| 位置 | 职责 |
|---|---|
| `src/IME/` | TSF 文本服务主体、激活/停用、消息窗口、后台 IPC 与主题监听生命周期 |
| `src/Key/` | `ITfKeyEventSink` 按键预判、异步按键队列与 edit session 分发 |
| `src/Composition/` | 预编辑缓冲、光标、按键表、preserved key、输入模式与标点/全半角状态 |
| `src/Edit/` | TSF edit session、文本范围和 composition 写入 |
| `src/UI/`、`src/Candidate/` | TSF 候选 UI element、候选状态、候选提交与布局通知；可见候选窗主要由 Server 绘制 |
| `src/IPC/` | Main / ToTsf / Worker / Aux Named Pipe 协议、握手、请求配对和重连状态 |
| `src/Thread/`、`src/Tf/` | 焦点、context stack、文本布局等 TSF sink |
| `src/Compartment/`、`src/LanguageBar/` | 中英文、标点、全半角 compartment 与语言栏按钮 |
| `src/Register/`、`src/DisplayAttribute/` | COM/TSF 注册及预编辑显示属性 |
| `src/Global/`、`src/Header/`、`src/Utils/` | 全局状态、资源/常量、COM 与 Win32 工具 |
| `src/DllMain.cpp`、`src/Server.cpp` | DLL 入口、class factory、引用计数和注册导出 |
| `image/`、`src/IME/MetasequoiaIME.rc` | DLL 内嵌图标与资源 |
| `scripts/` | 环境生成、Debug/Release 双架构构建、注册和签名脚本 |

## 核心输入链路

```text
宿主按键
  → OnTestKeyDown / _IsKeyEaten（同步决定是否吃键）
  → OnKeyDown / _DispatchKeyDown
  → Main Pipe 发给 Server
  → request_id 对应的回复或异步 Worker Pipe 消息
  → 消息窗口回到 TSF 所属线程
  → ITfEditSession 修改 composition / 提交文本
  → TSF UI element 与 Server 候选窗同步
```

查问题时先沿这条链路定位。按键判定主要看 `src/Key/KeyEventSink.cpp`，编辑行为看
`src/Key/KeyHandler.cpp` 与 `src/Key/KeyStateCategory.cpp`，协议与连接状态看 `src/IPC/Ipc.*`，
激活、焦点和工作线程生命周期看 `src/IME/MetasequoiaIME.*` 与 `src/Thread/`。

## TSF / COM 硬约定

- **Test 与实际处理必须一致**：`OnTestKeyDown` 决定宿主是否还能收到按键。一旦返回“吃掉”，后续
  IPC 超时或处理分支不能再把该键交还宿主。新增按键或模式时，同时检查 fresh composition、普通
  composition、候选态、离线 fallback 和 `_DispatchKeyDown` 的实际分支，避免丢键或重复上屏。
- **文档修改只在 edit session 内完成**：composition 的开始、更新、提交和删除必须通过
  `ITfContext::RequestEditSession` 获得有效 cookie；后台线程和窗口回调不得直接写 TSF range。
- **TSF 对象遵守 apartment 与线程归属**：不要把 `ITfContext`、`ITfComposition`、presenter 等 COM
  对象交给后台线程直接调用。跨线程结果通过现有隐藏消息窗口、`PostMessage` 和队列回到 owner
  线程；同步 fallback 只能沿用已有且已验证的路径。
- **异步回调必须验证所属会话**：延迟按键和 Server 回复可能晚于失焦、composition 结束或 DLL
  停用。应用前核对现有的 focus token / generation、composition epoch、local reset token、
  deferred replay token；过期结果应丢弃，不能作用到新会话。
- **引用与 teardown 成对**：新增 COM 接口、sink、HANDLE、线程、窗口或 presenter 时，在所有失败
  分支与停用路径补齐 `Release`、Unadvise、关闭和 join。不要在析构/停用后留下会访问 `this` 的
  posted message 或 watcher。
- **`DllMain` 保持最小化**：它处于 loader lock 下，禁止在其中连接管道、启动 Server、创建线程、
  等待锁或做复杂初始化。重活放到文本服务激活后的既有生命周期中。
- **DLL 会注入第三方宿主进程**：保持静态 CRT（`MultiThreaded[Debug]`）与
  `/Zc:threadSafeInit-` 约定，避免和 QQ、浏览器等宿主的 CRT/TLS 冲突；不要随意增加全局构造器、
  进程级 hook 或修改宿主状态。

## IPC 协议——跨仓单一契约

协议定义的唯一来源是 `vendor/MetasequoiaImeEngine/contracts/`，Server 通过自己的 Engine 子模块读取同一份定义。TSF 仅引用头文件，不链接引擎运行库；`product-lock.json` 和产品 CI 强制两端引用相同的 Engine 提交。这里保留已发布的固定宽度 Win32 ABI：

- 修改管道名、opcode、字段、容量或对齐时，在共享 contracts 中修改并保留 ABI 检查，再同步更新两端子模块和产品清单。禁止恢复两份手写定义。
- 主连接的版本化 ClientHello 必须获得关联 request_id 的 ProtocolReady 后才可发键；协议确认包不能作为候选或上屏文本。新 Server 兼容旧 DLL 的未版本化 hello；新 DLL 对旧或不兼容 Server 使用现有原始输入回退。版本与能力规则见共享 contracts/README.md。
- `WCHAR` 按 16 位 UTF-16 码元传输；不要用 `wchar_t` 在非 Windows 平台上的大小推断协议布局。
- 已发布 opcode 只追加、不复用、不重排。未知的较新 Worker opcode 应忽略，不能因此拆除连接。
- `request_id == 0` 表示 unsolicited，`UINT64_MAX` 表示本地无请求；不得当成普通请求回复配对。
- `PipeReady` 是反向管道握手帧，只在注册阶段消费；Worker handle 在握手完成前不能发布，Main pipe
  也不能抢先进入可用状态。
- Server 是候选选择、翻页与配置化导航的权威。TSF 本地影子状态用于预判和流畅回放，不能在回复
  到达后反向覆盖 Server 已更新的候选状态。
- 区分 `DefinitelyNotSent` 与 `DeliveryAmbiguous`：后者可能已被 Server 接收，不能无条件本地重放，
  否则会双写。`TransportUnavailable` 是本地控制结果，绝不能当候选文本提交。
- 改协议后至少构建 TSF 的 x64/x86，并运行 Server 侧的 IPC 协议测试
  `tests/src/test_ipc_protocol_constants.cpp` 所属测试目标。

## 焦点、候选与离线回退

- TSF 激活/停用与窗口瞬时失焦不是同一事件。切换到别的输入法使用 `ClientDeactivated`；宿主内部
  的临时焦点变化使用 suspended/reset 流程。Excel 单元格编辑、浏览器和 Windows Text Input Host
  对焦点的处理不同，修改时都要回归。
- 候选 presenter teardown 时要先隐藏并清除**当前 Server 会话**，同时防止旧 cleanup 消息清掉
  新 composition 的候选；保留现有 session-active 与 async-cleanup 防护。
- Server 不可用时只能提交 TSF 已确定拥有的本地原始输入。没有候选权威时不要猜测候选文本；服务
  恢复后先完成 focus session 激活/reset，再排空 deferred keys。
- CN→EN、标点和全半角切换同时涉及 preserved key、TSF compartment、语言栏和 Server 状态。
  composition 中切换模式时须保持“先正确提交，再应用 compartment”的既有顺序，避免宿主双提交。
- `raw` / `pinyin` / `empty` 预编辑样式定义在 `src/Global/FanyDefines.h`；新增样式时同步检查配置读取、
  caret 映射和 Server 返回的 `Preedit` 消息，不能只改显示字符串。

## 资源、注册与日志

- CLSID、profile GUID、compartment GUID、语言栏 GUID 与注册表路径属于持久身份。除非明确进行迁移，
  不要重新生成或改写；改动时同时核对 `Register/`、`Globals.*`、`.rc` 与卸载路径。
- 新增/替换图标时同步资源 ID、`.rc`、`CMakeLists.txt` 和 `LanguageBar` 的索引；浅色/深色图标应成对。
- 生产日志不得记录用户原始按键、预编辑、候选词或提交文本。性能与 IPC 诊断记录耗时、请求 ID、
  token、状态和错误码即可；调试日志也应尽量避免持久化用户输入。

## 生成文件与配置真源

`python .\scripts\prepare_env.py` 会覆盖根目录的 `.clangd`、`CMakeLists.txt` 和
`CMakePresets.json`。因此：

- `CMakeLists.txt` 的长期改动要同步到 `scripts/config_files/CMakeLists.txt`；
- `CMakePresets.json` 的结构改动要同步到 `scripts/config_files/CMakePresets.json`；
- `.clangd` 的 include 列表改动要同步到 `scripts/config_files/.clangd` 或修改生成脚本；
- `CMakeUserPresets.json` 保存 x64/x86 与 Debug/Release 的用户层 preset；不要提交新的个人绝对路径；
- `build*`、`.venv` 等均为本地产物，不要加入版本控制。

`prepare_env.py` 当前按固定行号替换模板内容。调整模板前要同时检查脚本索引，避免生成出语法正确但
字段错位的配置。

## 代码风格

- 使用 C++17、Windows Unicode API、SAL 标注和项目现有的 4 空格 Microsoft 风格。
- 格式化遵循根目录 `.clang-format`，且明确 `SortIncludes: false`；不要自动重排 include。
- 修改 `.cpp` / `.h` 后对**本次涉及的文件**运行：

```powershell
clang-format -i .\src\Path\Changed.cpp .\src\Path\Changed.h
```

- COM 出参失败时置空；检查 `HRESULT` / Win32 返回值；所有权不明确时优先沿用项目现有
  `SafeRelease`、RAII 或 teardown 模式，不要混用不兼容的智能指针约定。
- 不做与任务无关的大范围格式化或命名重构。TSF 状态机对时序敏感，小而可验证的改动优先。

注意：新增的代码尽量保持这个风格就可以了，不要去尝试格式化原有的代码。不严格要求新增代码保持这个风格。

## 构建与验证

环境：Windows、Visual Studio 2026、CMake 3.25+、vcpkg、Python 3.10+；首次准备：

```powershell
python .\scripts\prepare_env.py
```

Debug：

```powershell
.\scripts\lcompile.ps1 64
.\scripts\lcompile.ps1 32
```

Release：

```powershell
.\scripts\lcompile-release.ps1 64
.\scripts\lcompile-release.ps1 32
```

本仓的 `windows_ipc_contract` 在 x64/x86 上验证协议布局、版本握手与语音分帧；产品 CI 还构建锁定的 Server、页面并在锁定词库上运行 Server 测试。这些检查不能替代真实 TSF 宿主行为回归。按改动范围做手工回归：

- 至少验证 x64 宿主；协议、注册、资源或发布改动同时验证 x86 DLL；
- 输入首键、连续输入、空格/数字键盘选词、翻页、退格、光标移动和标点提交；
- composition 中与空 composition 下切换中英文、标点、全半角；
- 失焦/回焦、切换输入法、Server 未启动/重启/断管恢复；
- 涉及兼容性时覆盖 Win32 EDIT、Excel、Chromium 系浏览器及 UWP/现代 TSF 宿主。

本地注册脚本会执行提权后的 `regsvr32`，属于系统状态修改，不应作为普通验证自动运行。只有用户明确
要求安装/卸载时才执行：

```powershell
.\scripts\linstall.ps1
.\scripts\luninstall.ps1
```

签名脚本包含本机 Windows SDK 路径和证书占位符；没有对应证书时不要运行或提交真实 thumbprint。

需要生成可安装包做本地测试时，从各组件的共同父目录开始，严格按以下顺序执行。任一步失败都先
停止并修复，不要继续使用旧产物打包：

```powershell
Set-Location .\MetasequoiaImeTsf
.\scripts\lcompile-release-both.ps1

Set-Location ..\MetasequoiaImeServer
.\scripts\lcompile-release.ps1

# 使用 Installer 下最新的 msime_v<版本> 目录；当前示例为 0.0.7。
Set-Location ..\Installer\msime_v0.0.7
.\Prepare-PackageFiles.ps1
.\Sign-PackageBinaries-Local.ps1
.\Compile-Installer.ps1
```

`Prepare-PackageFiles.ps1` 会收集 Server Release、x86/x64 TSF DLL、词库和前端资源；因此必须在两个
C++ 组件编译完成且相关前端/词库产物已准备好之后运行。`Sign-PackageBinaries-Local.ps1` 使用本机
测试证书，只用于内部安装验证；它可能创建证书并尝试写入本机受信任存储，不得把本机证书指纹或
私钥提交到仓库。`Compile-Installer.ps1` 会调用 Inno Setup 命令行编译器 `ISCC.exe` 编译同目录的
`msime_setup.iss`，安装包输出到该版本目录的 `Output\`。机器上须预先安装 Inno Setup 6.6+；若脚本
无法自动找到编译器，可显式传入：

```powershell
.\Compile-Installer.ps1 -IsccPath 'C:\Program Files\Inno Setup 7\ISCC.exe'
```

## 正式发布（CI）

上面那套是本地测试打包。对外发布走 `.github/workflows/release.yml`，产出的就是历来挂在本仓 Release 上的 `MetasequoiaIME_Setup_v<版本>.exe`。

版本号由 release-please 管理，真源是根目录的 `version.txt`。**发布是手动的**：签名证书的签名次数有限，一次发布是实打实的开销，不该由「合了个 PR」这种事替你花掉。

往 `main` 推提交后只发生两件事：release-please 更新那个 release PR，`check-release-pr.sh` 给它挂上一次通过的 CI（`GITHUB_TOKEN` 开不出 workflow run，必须用 `workflow_dispatch` 顶上，否则开着 required status check 的 release PR 永远合不了）。想发版时:

1. 合并 release PR —— 这一步只生成 draft release 和 tag，不构建、不签名。
2. 手动触发 `Release` workflow，`tag` 填那个 draft 的 tag —— 这一步才构建、签名、发布。

同一个 tag 重复触发会被 `validate-draft-release.sh` 挡下：发布之后它就不是 draft 了，不会二次消耗签名次数。

`src/IME/MetasequoiaIME.rc` 的 `FILEVERSION` / `PRODUCTVERSION` 不由 release-please 直接改（它是逗号和点号两种写法），而是由 `scripts/apply_version.py` 从 `version.txt` 注入，release 构建在 configure 之前执行：

```powershell
python .\scripts\apply_version.py           # 从 version.txt 写入版本资源
python .\scripts\apply_version.py --check   # 只检查是否与 version.txt 一致
```

release workflow 里的每一段 shell 都抽在 `scripts/ci/` 下，workflow 本身只负责编排和传参：

| 脚本 | 用途 |
|---|---|
| `validate-draft-release.sh` | 手动触发时校验 tag 是未发布 draft、指向不可变 commit、且与 `version.txt` 一致 |
| `check-release-pr.sh` | 给 release PR 挂上一次通过的 CI，不合并 |
| `verify-commit-on-main.sh` | 拒绝构建不在 main 历史里的 commit |
| `install-boost.ps1` | 装 Server 链接但未声明的 Boost，triplet 必须是 static-md |
| `check-server-binaries.ps1` | 提前拦住 Server 产物缺文件 |
| `check-tsf-dll.ps1` | 确认 TSF DLL 产出 |
| `download-dictionaries.sh` | 从 MSIME-Dict 的 `dict-*` release 拉词库并校验 SHA256 |
| `detect-release-signing.ps1` | 判定签名模式，决定产物后缀 |
| `sign-binaries.ps1` | 用仓库 secret 里的真证书签名，包内二进制和安装包共用 |
| `install-inno-language.ps1` | 补 runner 上缺失的 `ChineseSimplified.isl`，按 commit + SHA256 固定。装到真正的 Inno Setup 安装目录，不是 Chocolatey shim 旁边 |
| `check-inno-language.ps1` | CI 用：编译一个只含 `[Languages]` 的探针脚本，让 ISCC 自己回答语言文件放对没有 |
| `name-installer-asset.ps1` | 定最终产物名、算校验和、写 step summary |
| `revalidate-draft-release.sh` | 发布前复查 draft 仍指向被构建的那个 commit |
| `publish-release.sh` | 上传产物、追加说明、发布 |

CI 里的目录布局刻意用了各仓的历史名（`MetasequoiaImeTsf`、`MetasequoiaImeServer`、`MetasequoiaImeUiHtml`、`MetasequoiaImeHelpCode`、`MetasequoiaImeDict`），这样 `msime-installer` 的 `Prepare-PackageFiles.ps1` 不用改一行就能在 CI 跑。改动那些脚本里的产物路径时，要连同本仓的 release workflow 一起核对。

词库不在 CI 里现建，从 `metasequoiaime/MSIME-Dict` 的 `dict-*` release 下载并校验 SHA256。词库改了要先在那边跑 `Build dictionaries` workflow 并勾选 publish，再发 Windows 版本；通过 `scripts/product_lock.py refresh --dictionary-tag <tag>` 更新产品锁并评审摘要变更；发布构建不能临时覆盖词库版本。

签名沿用「有证书就签、没有就发未签名版」的策略：配置了 `WINDOWS_SIGNING_CERTIFICATE_BASE64` 和 `WINDOWS_SIGNING_CERTIFICATE_PASSWORD` 两个 secret 时，用 `signtool` 签包内 EXE/DLL 和安装包；没配置时产物名带 `-unsigned` 后缀，并在 release 说明里写明 `uiAccess` 不会生效。`Sign-PackageBinaries-Local.ps1` 使用本机自签名证书，只用于本地验证，CI 不会调用它。

## 提交纪律

仓库可能与相邻 Server/UI 仓库同时改动。提交前分别在各仓执行 `git status --short`，只暂存本任务
涉及的显式路径，禁止 `git add -A` / `git add .` 把构建产物或其他会话改动卷入提交。

提交信息沿用项目现有工程风格，推荐 `type(scope): 摘要`。不要添加 `Co-Authored-By`、
`Generated with` 或其他 AI 生成标记。

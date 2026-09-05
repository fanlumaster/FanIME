# AGENTS.md — MSIME-Server

产品级约定、跨仓契约与共享数据规则以 [MSIME-Windows 的 AGENTS.md](https://github.com/metasequoiaime/MSIME-Windows/blob/main/AGENTS.md) 为准，那一份统领整个水杉输入法项目，本仓的角色、IPC 协议、窗口与 WebView2 边界、DPI 约定都写在那里。本文件只补本仓内部容易踩空的几处。

本仓是常驻后端进程：输入引擎与候选状态、配置、词典、Named Pipe 服务，以及候选窗、悬浮工具栏、托盘菜单、设置窗口的原生宿主与 WebView2 控制器。

## vcpkg_installed 的解析

`CMakeLists.txt` 四处引用 `vcpkg_installed`，全部走 `${CMAKE_BINARY_DIR}`。其中 `WEBVIEW2_VCPKG_ROOT` 供 WebView2 loader 的导入库和运行时 DLL 拷贝使用。

**不要改回 `${CMAKE_SOURCE_DIR}/build/...`。** 那样 binary dir 就只能叫字面的 `build`，本仓自带的 `vcpkg-release` preset（`binaryDir` 是 `build-release`）和 `scripts/lcompile-release.ps1` 都会在链接 `MetasequoiaImeSettings` 时报 LNK1181 找不到 `WebView2Loader.dll.lib`。

CI 刻意构建到 `build-release` 而不是 `build`，为的就是让这条不会悄悄退回去——两者在目录名恰好是 `build` 时解析结果相同，所以只有构建到别处才测得出来。

## 测试跑在真实词库上，本地要先备好数据

`tests/CMakeLists.txt` 构建 `MetasequoiaImeServerTests`，`tests/src/` 下有 34 个测试源文件。根 `CMakeLists.txt:8` 的 `include(CTest)` 让 `BUILD_TESTING` 默认为 ON，`:420` 据此 `add_subdirectory(tests)`，而 `tests/CMakeLists.txt:78` 用 `add_test` 注册了一条同名用例。

CI 现在会真的跑它：`.github/workflows/ci.yml` 在 Build 之后有 Provision dictionary data 和 Test 两步，后者执行 `ctest --test-dir build-release -C Release --verbose --timeout 120`。断言失败会让 CI 变红，不再是编译通过就算数。

**关键在于这些测试依赖真实词库，不是 fixture。**引擎从数据根目录读数据，默认 `%LOCALAPPDATA%\metasequoiaime`，可用 `METASEQUOIA_IME_DATA_DIR` 环境变量覆盖。CI 的 Provision 步往那里放了四样东西，本地跑测试要凑齐同样的：

- `msime.db`、`others.db`、`english.db`——从 MSIME-Dict 的 release 下载，CI 逐个核对 `SHA256SUMS.txt`。词库损坏要立刻失败，而不是拖到测试里表现成「候选为空」这种难查的样子
- `helpcodes/`——五套辅助码方案，本仓 `assets/tables` 只有其中一套，得从 MSIME-HelpCode 取全
- `assets/tables/*` 和 `assets/config/config.toml`

数据不全时的典型症状是候选查询返回空集，断言信息看起来像逻辑错误，实际是缺数据。排查测试失败前先确认数据根目录是齐的。

IPC 线格式、opcode 和语音分帧的唯一实现位于 Engine 子模块的 `contracts/`；本仓的 `src/ipc/ipc.h` 只保留连接状态和 Server API。不要重新复制协议定义。共享头的 ABI 断言随主工程编译，主连接必须通过 `NegotiateMainPipeClient` 后才能激活；旧 DLL 的未版本化 hello 仍兼容，版本化客户端会收到关联 request_id 的协议确认。

改了协议或候选逻辑，正常构建并备好数据之后自己跑一次：

```powershell
ctest --test-dir build-release -C Release --output-on-failure
```

## uiAccess 与 Topmost 时序

`MetasequoiaImeServer.manifest` 用 `requestedExecutionLevel level="asInvoker" uiAccess="true"`，浮层才能盖在高完整性宿主之上。`uiAccess` 真正生效还依赖正确签名和安装在 Windows 认可的位置，清单、签名、安装路径是同一套发布约定。

进程在 `src/main.cpp:90` 设 `DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2`。

**创建 HWND 时不能直接带 `WS_EX_TOPMOST`。** 在 `uiAccess=true` 进程里，Topmost 父窗口会让 WebView2 的跨进程 `SetParent` 返回 `E_INVALIDARG`，或者形成「窗口存在、尺寸正确但永远空白」。现有顺序是：非 Topmost 建窗 → DWM cloak 下预热 → 三个共享 environment 的 controller 依次创建并完成首屏导航 → lazy-topmost gate 打开 → 分阶段提升到 `HWND_TOPMOST`。提升后要用 `RenotifyControllerAfterPin` 重新同步 bounds 和 parent position。不要在布局函数里随手传 `HWND_TOPMOST`，也不要绕过 `EnsureSmallWindowsTopmost` / lazy pin。

隐藏尚未完成首帧的 WebView2 host 会阻断 raster 初始化，预热阶段用 cloak，之后才按配置显示或隐藏。

## Boost

`CMakeLists.txt:13` 的 `Boost_ROOT` 是有守卫的：设了 `BOOST_ROOT` 环境变量就用环境变量，否则回退到作者本机路径。**这是有意的，不要改成只认环境变量**，那会破坏作者的本地环境。CI 就是靠设这个环境变量工作的。

Boost 是静态链接但没有写进 `vcpkg.json`，所以只能用 classic 模式装，且 triplet 必须是 `x64-windows-static-md` 而不是 `x64-windows-static`——`Boost_USE_STATIC_LIBS ON` 要静态 Boost 库，而项目其余部分用动态 CRT，纯 static triplet 会让 Boost 也切到静态 CRT，链接时报 LNK2038。

## 生成文件

`scripts/prepare_env.py` 只生成两个文件：根目录的 `.clangd`（从 `scripts/config_files/.clangd` 模板展开），以及 `tests/CMakePresets.json`（从 `scripts/config_files/tests/CMakePresets.json` 展开，那个文件不入版本库，模板是唯一的一份）。`CMakePresets.json` 是**原地**改写 `VCPKG_ROOT` 与 `CMAKE_TOOLCHAIN_FILE`，不再从模板覆盖。

`CMakeLists.txt` 和 `tests/CMakeLists.txt` 不再由脚本生成，直接改就行。这两个文件曾经也是从 `scripts/config_files/` 覆盖过来的，但模板早已落后到 147 行对 400 行、以及一个无关的通用 `WinCppTemplate`，跑一次脚本就会毁掉构建，因此模板已删除。

`.clangd` 模板用 `@NAME@` 占位符，不再按固定行号替换——旧的行号索引已经漂移过一次，把 Boost 路径写到了 WebView2 的 include 上。新增 include 时在模板里加占位符、并在脚本的 `clangd_substitutions` 里补上对应项即可；有占位符没被替换脚本会直接报错。

## 提交

提交信息用 `type(scope): 摘要`。不要添加 `Co-Authored-By`、`Generated with` 或其他 AI 生成标记。

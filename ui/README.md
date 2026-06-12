# msimeui

`msimeui` 是一个基于 Win32 的 C++ GUI 工程，渲染层使用 Direct2D / DirectWrite，文本输入使用 TSF。

- 常规界面元素使用 Direct2D / DirectWrite 渲染
- 使用原生 Win32 窗口作为宿主
- 文本输入控件内置 TSF 实现
- 界面结构基于控件树和布局系统

## 当前内容

当前仓库包含：

- `msimeui` 静态库
- `msimeui-demo` 示例程序
- D2D/DWrite 设备资源管理
- 基础布局和控件树
- `TextBox` 输入控件

## 构建

如果已经配置好本机的 `vcpkg` 路径，可以直接使用 preset：

```powershell
cmake --preset vcpkg-debug
cmake --build --preset debug
```

如果要编译 `Release`：

```powershell
cmake --preset vcpkg-release
cmake --build --preset release
```

也可以继续使用普通 CMake 命令：

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

如果本机的 `vcpkg` 不在 `C:/Users/SonnyCalcr/scoop/apps/vcpkg/current`，先按自己的环境修改 [CMakeUserPresets.json](C:/Users/SonnyCalcr/EDisk/CppCodes/IMECodes/RelatedProjects/msimeui/CMakeUserPresets.json:1) 里的 `VCPKG_ROOT` 和 `CMAKE_TOOLCHAIN_FILE`。

也可以直接使用 `scripts/` 里的 PowerShell 脚本：

```powershell
./scripts/configure.ps1
./scripts/build.ps1
./scripts/run-demo.ps1
./scripts/launch-demo.ps1
```

指定 `Release`：

```powershell
./scripts/build.ps1 -Configuration Release
./scripts/run-demo.ps1 -Configuration Release
./scripts/launch-demo.ps1 -Configuration Release
```

如果切换过 CMake generator、Visual Studio 版本或者 preset，可以加 `-Fresh` 重新生成构建目录：

```powershell
./scripts/build.ps1 -Fresh
./scripts/launch-demo.ps1 -Fresh
```

生成程序默认位于：

```text
build/bin/Debug/msimeui-demo.exe
```

## 目录

- `include/msimeui`：新框架公开头文件
- `src`：框架实现
- `demos/msimeui-demo`：demo 入口和示例场景
- `src/tsf`：内置 TSF 文本输入实现

## 后续工作

- 继续完善 `TextBox` 的输入、样式和布局能力
- 增加按钮、滚动容器、列表和样式系统
- 引入资源字典、主题和声明式界面描述

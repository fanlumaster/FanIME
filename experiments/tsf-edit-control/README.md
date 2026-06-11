# TsfEditControl

一个独立的 Win32 TSF 编辑控件工程，当前实现基于 Direct2D / DirectWrite 渲染，并把 TSF 文本输入、组合串绘制、候选框定位和软换行都收进了控件内部。

## 当前能力

- Direct2D / DirectWrite 文本渲染
- TSF 文本服务接入
- preedit / display attribute 绘制
- 候选框位置上报
- 软换行
- 基础选区、插入点与鼠标命中
- 独立 demo 宿主程序

## 目录结构

- `include/TsfD2DTextBox.h`
  当前公开头文件。
- `src/`
  控件内部实现与 TSF 支撑代码。
- `demo/TsfD2DTextBoxDemo.cpp`
  最小宿主示例。
- `CMakeLists.txt`
  顶层工程文件，可直接单独配置。
- `vcpkg.json`
  当前依赖清单。
- `CMakePresets.json`
  预置的 VS2022 + vcpkg 配置。

## 生成目标

- `TsfEditControl`
  静态库目标。
- `TsfD2DTextBox`
  兼容别名目标，方便旧命名逐步迁移。
- `TsfEditControlDemo`
  demo 可执行文件。

## 构建

如果你已经在本机装好了 vcpkg，并且路径和 preset 里的设置一致，可以直接执行：

```powershell
cmake --preset vcpkg
cmake --build --preset debug
```

生成后的 demo 默认在：

```text
build/bin/Debug/TsfEditControlDemo.exe
```

## 现阶段说明

当前对外类型名还是 `TsfD2DTextBox`，这是为了先保持 API 稳定。后续如果你决定正式把它演进成通用 edit 控件，我们可以再一起做一轮命名收口，把：

- 文件名
- 类名
- CMake target 名
- demo 名称

统一成 `TsfEditControl` 风格。

## 脚本

`scripts/` 目录里现在提供了几份 PowerShell 脚本：

- `./scripts/configure.ps1`
  一键执行 CMake configure。
- `./scripts/build.ps1`
  一键配置并编译，默认 `Debug`。
- `./scripts/run-demo.ps1`
  一键配置、编译并启动 demo。

示例：

```powershell
./scripts/build.ps1
./scripts/build.ps1 -Configuration Release
./scripts/run-demo.ps1
```

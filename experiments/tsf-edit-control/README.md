# TsfEditControl

一个基于 Win32 TSF 的编辑控件实验工程，使用 Direct2D / DirectWrite 负责绘制。项目里把文本输入、组合串显示、候选框定位和基础编辑逻辑放进了控件本身，demo 只是一个最小宿主。

## 功能

- Direct2D / DirectWrite 文本渲染
- TSF 输入法接入
- preedit / display attribute 绘制
- 候选框位置上报
- 软换行
- 选区、插入点和鼠标命中
- 独立 demo 程序

## 目录

- `include/TsfD2DTextBox.h`：对外头文件
- `src/`：控件实现和 TSF 相关代码
- `demo/TsfD2DTextBoxDemo.cpp`：最小示例程序
- `CMakeLists.txt`：顶层 CMake 配置
- `CMakePresets.json`：VS2022 + vcpkg preset
- `vcpkg.json`：依赖声明

## 构建

如果本机已经装好了 vcpkg，并且 preset 里的路径配置可用，可以直接执行：

```powershell
cmake --preset vcpkg
cmake --build --preset debug
```

生成的 demo 默认位于：

```text
build/bin/Debug/TsfEditControlDemo.exe
```

## 生成目标

- `TsfEditControl`：静态库
- `TsfD2DTextBox`：兼容旧命名的别名 target
- `TsfEditControlDemo`：demo 可执行文件

## 命名说明

对外类型名暂时还是 `TsfD2DTextBox`，主要是为了不动现有接口。工程名和 target 已经改成了 `TsfEditControl`，后面如果要统一命名，再一起收口文件名、类名和 demo 名称。

## 脚本

`scripts/` 目录提供了几个常用脚本：

- `./scripts/configure.ps1`：执行 CMake configure
- `./scripts/build.ps1`：配置并编译，默认 `Debug`
- `./scripts/run-demo.ps1`：配置、编译并启动 demo

示例：

```powershell
./scripts/build.ps1
./scripts/build.ps1 -Configuration Release
./scripts/run-demo.ps1
```

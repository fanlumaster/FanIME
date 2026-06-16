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
- 共享布局属性：`margin` / `padding` / 显式尺寸 / `min-max` 约束 / 对齐
- `StackPanel` / `HorizontalStackPanel` / `WrapPanel` / `ScrollViewer` / `Grid`
- 通用单子元素容器：`Container` / `Border`
- `TextBlock` / `Button` / `CheckBox` / `ProgressBar` / `Slider` / `Separator` / `ListView` / `TreeView` / `TabControl` / `Accordion`
- `ComboBox`
- `ContextMenuHost`
- `Card`
- `TextBox` 输入控件

## 最近新增

- demo 页面支持滚轮滚动，便于测试长页面和底部控件
- 基础控件首帧文字测量修复，按钮初始宽度不会再异常撑满
- 窗口图标已接入 `assets/msimeui.ico`
- 所有 `Visual` 现支持共享布局属性：
  `margin`、`padding`、`width/height`、`min/max width/height`、水平/垂直对齐
- `StackPanel` 和 `HorizontalStackPanel` 现支持内容级对齐
- 新增通用 `Container` / `Border`，可以在不依赖 `Card` 的情况下组织带内边距和边框的单子元素区域
- 新增 `Grid` 布局，支持 `Auto` / `Pixel` / `Star` 三种轨道尺寸模式，适合表单、设置页和双栏内容区
- `ScrollViewer` 现支持可见纵向滚动条，并可通过滚轮和拖拽滚动条滑块进行滚动
- 新增基础 `ListView`，支持垂直列表项绘制、选中态和点击切换，适合导航列表和设置摘要区
- 新增基础 `TreeView`，支持层级节点、展开折叠、选中态和点击切换，适合导航树和模块浏览区
- 新增基础 `TabControl`，支持页签头、选中态和内容区切换，适合设置页、工具面板和分组工作区
- 新增基础 `Accordion`，支持折叠分组、展开收起和单开/多开模式，适合长设置页和分组内容区
- 修复了 `TabControl` / `TreeView` / `Accordion` 这类会改变内容结构的控件在切换后未及时重新布局而导致的稳定性问题
- 修复了单行 `TextBox`（如 demo 中的 Search Box）输入光标和文本区域未垂直居中的问题
- 修复了单行 `TextBox` 在垂直居中后输入法候选框未同步跟随文本区域下移的问题
- 新增一版场景级 invalidation 机制：控件现可通过 `InvalidateMeasure` / `InvalidateArrange` / `InvalidateVisual` 统一声明布局或重绘失效，`Scene` / `Window` 会在下一次绘制前自动补做布局，减少到处手动 `Relayout()` 的耦合
- 补充了控件树父子关系和冒泡式失效传播：容器在收养子控件时会建立 parent 链，子控件的布局/重绘失效会先沿控件树向上汇聚，再交给根场景统一处理，为后续做局部布局优化打基础
- 在 `Visual` 基类中加入了 `MeasureInLayout` / `ArrangeInLayout` 缓存：当 available size 和 final rect 没变化且控件未标脏时，会直接复用上次布局结果，减少重复测量和排列
- `Window` 新增了按矩形区域失效的入口，`Visual::InvalidateVisual()` 现在会优先刷新自身边界区域；按钮、列表、树、页签、折叠面板和 `TextBox` 等常见交互控件已开始接入局部重绘
- 继续收敛高频整窗刷新：`CheckBox` 已切到控件级失效，`ScrollViewer` 的拖拽滚动条和滚轮滚动现在会优先刷新自身视口区域，而不是默认请求整窗重绘
- `Scene::Layout` 现会区分 `measure dirty` 和 `arrange dirty`：仅排列脏时会直接复用上一轮测量结果，只执行 `Arrange`，避免每次布局都无条件重测整棵树
- 新增一版真正参与布局调度的 `arrange subtree dirty`：`Visual::InvalidateArrange()` 现在会把源控件一路上传到 `Scene`，`Scene` 会记录需要重排的最小公共祖先；在无需重测时，会优先只重排那一支子树，而不是整棵根树
- 进一步补上 `measure dirty` 的源追踪：`Scene` 现在会记录测量脏源，并在条件允许时优先按脏路径重测，再结合 `Visual` 的布局缓存尽量跳过无关分支
- 新增最小主题系统 `Theme / ThemeManager`：窗口背景、按钮、复选框、进度条、滑块、滚动条、`TextBlock` 字体和 `TextBox` 的部分视觉令牌已开始迁移到统一主题入口，便于后续做换肤和样式集中管理
- 新增第一版 `PopupHost / Popup` 弹出层系统：`Scene` 现可管理 overlay popup，支持高于主视觉树的渲染顺序、命中测试、滚轮路由、窗口重排后的重新定位，以及点击空白区域自动关闭；demo 已加入一个快速操作弹出层示例
- 新增第一版 `ComboBox`：基于新的 overlay popup 系统实现下拉选择，支持展开/收起、点击项选中、当前值回写，以及点击空白区域自动关闭；demo 已加入可交互示例
- 新增第一版 `ContextMenuHost`：窗口现可分发右键上下文菜单事件，控件可在鼠标位置弹出 scene overlay 菜单，并支持点击菜单项执行动作、点击空白区域自动关闭；demo 已加入右键菜单示例
- 新增第一版渲染对象缓存：`DeviceResources` 现会缓存常用 `SolidColorBrush` 和 `TextFormat`，公共文本/圆角矩形绘制 helper、`TextBlock`、`TextBox`、滚动条、`Border` / `Card` 以及多种基础控件已切到缓存路径，减少每帧重复创建 D2D / DWrite 对象的开销
- `TextBlock` 现进一步复用自身的 `IDWriteTextLayout`：同一段文本在宽度和主题字体未变化时，会复用上一轮排版结果，测量和渲染不再各自重复创建文本布局对象
- `Button` / `ListView` / `TreeView` 现也开始缓存各自高频文本的 `IDWriteTextLayout`，减少按钮标题、列表项标题/副标题/徽标，以及树节点标题/副标题在重复绘制时的文本排版开销
- 微调了 demo 中 `Interactive Controls` 卡片底部两枚示例按钮的上边距，让说明文字和按钮组之间的留白更自然
- 微调了单行 `TextBox` 的水平文字内边距，让表单输入框中的光标、占位文字和输入文本不会显得过于贴近左边框

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

- 继续把场景级 invalidation 细化为局部布局和局部重绘，减少全树重排成本
- 基于现有 parent 链继续引入更细粒度的 subtree dirty 标记和可缓存测量结果
- 为高频交互控件继续补局部重绘区域计算，而不是默认整窗 `InvalidateRect`
- 继续完善 `TextBox` 的输入、样式和布局能力
- 增加菜单、弹出层、overlay 和更完整的命中/焦点管理
- 引入资源字典、主题和声明式界面描述

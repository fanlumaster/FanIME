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
- `CandidateList`
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
- 微调了 demo 中 `TabControl` 的 Overview 页内容区：增加内部留白，并把示例按钮改成更合适的固定宽度，避免文字贴近圆角和按钮整行铺满
- 继续修正了 demo 中 `TabControl` 的 Overview 页左侧内容对齐：改为由外层内容容器提供统一 padding，让说明文字和按钮整体向右收齐
- 修复了 demo 中 `PopupHost / Popup` 示例的内容边距和滚动联动：弹出层内部留白与按钮左对齐更自然，并且在页面滚动时会跟随触发按钮重新定位
- 继续微调了 demo 中 `PopupHost / Popup` 示例里说明文字和操作按钮之间的垂直间距，避免长文本与按钮视觉重叠
- 继续增大了 demo 中 `PopupHost / Popup` 示例里说明文字和第一颗操作按钮之间的间距，让说明区和操作区的分段更清晰
- 调整了 `ComboBox` 下拉项之间的垂直间距，让每个选项的高亮背景之间留出缝隙，避免切换或悬停时矩形直接挤在一起
- 微调了 `Accordion` 标题栏的水平文字内边距，让折叠项标题不会再紧贴左侧圆角边缘
- 将 demo 中的 `Accordion` 示例切换为可多项同时展开，更直观展示该控件的多开能力
- 移除了 demo 中 `Accordion` 示例的固定高度，避免多项同时展开后内容文本被裁切和溢出
- 微调了多行 `TextBox` 的水平文字内边距，让大尺寸编辑区中的光标和首行文本不会显得过于贴近左边框
- 将 demo 顶部的 `msimeui` 标题和副标题改为居中展示，并把标题上方留白收窄到原来的一半
- 修正了 demo 顶部标题区的居中方式：改为通过独立 header 容器居中内容，确保 `msimeui` 标题和描述文本在页面顶部真正水平居中
- 为 `TextBlock` 补充了文本水平对齐能力，demo 顶部标题区现改为使用真正的居中文本对齐，而不是仅靠外层布局盒子居中
- 调整了 `ScrollViewer` 的纵向滚动条轨道边距，去掉上、下、右侧 inset，让滚动条更贴合视口右边缘
- 继续统一了 demo 中 `TabControl` 三个页签的内容区内边距，让 `Layout` / `Navigation` 页也与 `Overview` 一样保持自然的左侧留白
- 去掉了 `Scene` 层统一施加的四周外边距，并把 demo 根内容改为仅保留等量的左侧和底部内边距，让内容区更贴边但仍保留必要留白
- 修正了 `ComboBox` 重新展开时的默认高亮行为：弹出列表现在会优先高亮当前已选项，而不是停留在首项或旧 hover 状态
- 修复了 `ComboBox` 在弹出列表中更改选择后未及时同步内部选中态的问题，避免重新展开时出现旧选项和新选项同时高亮
- 调整了 `TreeView` 各层级节点背景框的左侧起点：子节点的圆角矩形现在会随缩进一起右移，更贴合本层内容的实际包裹范围
- 为 `TreeView` 补充了竖向层级引导线：祖先层级列和展开节点的子树主干线现在都会显示，层级结构更直观
- 微调了 `TreeView` 每个节点左侧的内部留白和引导线位置，让竖向层级线与下一层级节点边框之间留出更自然的间距
- 单行 `TextBox` 现在支持基于光标位置的横向自动滚动：当文本向右溢出时，左右箭头、输入插入、删除和鼠标定位都会自动把当前 caret 滚动到可见区域
- 修复了单行 `TextBox` 启用横向滚动后 preedit 组合下划线未跟随文本一起偏移的问题，输入法预编辑样式现已恢复正常显示
- 继续修正了单行 `TextBox` 的横向滚动回收逻辑：当末尾删除文本导致内容变短时，会及时收回多余的右侧滚动量，避免随后输入法预编辑时视图突然跳到最右端
- 修复了 `TextBox` 用左右方向键移动时中间位置 caret 被绘制成整字符方块的问题：插入光标现在在整行范围内都会保持细竖线样式
- 微调了 `TextBox` caret 的绘制位置：插入光标现在以字符边界为中心绘制，不会再明显压到后一个字上
- 继续微调了 `TextBox` caret 的视觉位置：在保持字符边界对齐的前提下，插入光标额外轻微向左偏移，和后一个字之间会留出更自然的呼吸感
- 修正了单行 `TextBox` 在文本溢出时按左箭头的横向滚动行为：现在会优先让 caret 正常向左移动，只有文本整体变短时才回收多余的右侧滚动量
- 修复了单行 `TextBox` 在文本溢出时 caret 闪烁导致整段文本左右抖动的问题：纯 blink 重绘不再重复触发布局更新和滚动校正
- 调整了 `ScrollViewer` 的鼠标样式：悬浮在滚动条轨道或滑块上时不再切到手型，继续保持普通箭头光标
- 优化了 `TextBox` 文本选区高亮的绘制方式：同一行中的连续选中文本现在会合并为更平滑的一整段背景，不再出现明显的逐字断裂感
- 为 `TextBox` 补上了基础快捷键支持：现在可以使用 `Ctrl+A` 全选、`Ctrl+C` 复制、`Ctrl+X` 剪切、`Ctrl+V` 粘贴，交互更接近常规输入框
- 优化了多行 `TextBox` 的选区高亮衔接：跨行选中时现在会按完整行带连续铺开，减少上下行之间的断裂感；同时补上了简单的 `Ctrl+Backspace` 删除上一个词能力
- 修复了 `Ctrl+Backspace` 的连续删除问题：过滤掉组合键路径里额外产生的 `DEL (0x7F)` 控制字符，避免第一次删词后又插入不可见字符，导致第二次按键看起来失效
- 调整了 demo 窗口的启动位置：现在会按主屏工作区居中创建，而不是沿用系统默认位置
- 进一步修正了多行 `TextBox` 的跨行选区接缝：选区背景的上下边界现在会贴齐像素网格，减少高亮行之间残留的细线
- 调整了 `ScrollViewer` 的滚动条悬浮反馈：鼠标移到滚动条轨道上时 thumb 就会立即切到高亮色，不再等到按下拖动后才变色
- 新增第一版面向输入法候选窗的 `CandidateList`：支持紧凑候选项、序号/主词/注解三段式排版、选中态切换，适合作为 WebView2 候选框的原生替代基础控件
- `Popup` 现支持自定义背景色、边框色、圆角半径和轻量阴影，便于构建深色候选窗、悬浮工具条弹层等更贴近输入法场景的 overlay UI
- demo 新增 `Candidate Window Target` 示例：使用深色 `Popup` + `CandidateList` 组合出一版接近输入法候选窗形态的候选层预览，作为后续替换 `MetasequoiaImeServer` WebView2 候选框的起点
- 继续收紧候选窗顶部标题区：压缩 `ni` 上方留白，并把拼音与首个候选项之间的距离调整到更接近参考图
- `TextBlock` 新增可调的文本布局内边距，候选窗顶部拼音标题已切换到紧凑测量模式，去掉了默认文本行高带来的额外顶部空白
- 候选窗 demo 中的顶部拼音 `ni` 再向下微调 `3px`，方便继续贴近参考图的标题基线位置
- 候选窗顶部留白现改为由 `Popup` 的 `2px` 上内边距统一控制，避免 `ni` 自身 margin 继续把顶部空白撑大
- 候选窗顶部拼音 `ni` 的下边距已清零，进一步压缩它与首个候选项之间的垂直空白
- 候选列表高亮态左侧的蓝色指示条已加粗，并改为占据高亮矩形约二分之一的高度，让选中态更接近参考候选窗
- 鼠标悬浮在候选框上时已保持默认箭头样式，不再切换成手型光标
- 候选列表现支持鼠标悬浮高亮：悬浮到不同候选项时会显示轻量 hover 背景，但不会覆盖当前首项的既有选中高亮和蓝色指示条
- 候选窗顶部拼音右侧已补上一根静态 caret 竖线，便于继续向真实输入法候选窗的标题区形态靠拢
- 修正候选窗顶部 `ni` 标题行的宽度占用，让右侧 caret 不再被文本布局挤出弹窗可视区域
- 候选窗顶部 `ni` 与 caret 的水平间距已收紧到约 `1px`，更贴近参考图中的输入光标位置
- 继续收窄候选窗顶部 `ni` 文本自身的占位宽度，避免 caret 虽然间距设小了但仍被文本槽位视觉上推远
- `Button` 点击后不再残留焦点高亮边框，保留按下态但去掉持续的描边强调
- `PopupHost` 现在会把按下/抬起鼠标事件转发给内部 trigger，因此像 `Open Quick Actions` 这样的弹层按钮也会显示正常的点击反馈
- `Popup` 新增是否限制在视口内的开关；demo 里的页面内弹层已关闭该限制，因此滚动页面时弹出框会跟随锚点自然滚出可视区，而不是停在窗口边缘
- `ComboBox` 下拉层默认也已关闭视口贴边限制，因此其滚动行为现在与页面内的其它 popup 保持一致
- 文本输入框现已接入 `assets/dict.txt` 词典来增强 `Ctrl+Backspace` / `Ctrl+Delete` 的按词删除：中文优先按最长词匹配删除，英文与数字仍按连续 token 删除
- `CandidateList` 现支持基础键盘导航：弹层打开后会自动把焦点落到首个可聚焦内容上，候选列表可用 `Up/Down`、`Left/Right`、`Home/End`、`PageUp/PageDown` 切换高亮项
- demo 中的候选窗预览继续补上了底部分页/状态提示区，用来预演候选页码、当前高亮项和键盘操作提示的布局结构
- `Window` 新增公开的 `FocusVisual` 入口，方便 popup、候选窗、上下文菜单这类 overlay 内容在弹出后主动把焦点切到内部可交互控件
- 修复了 demo 候选窗预览里的中文候选词乱码问题：示例候选词改为稳定的 Unicode 码点写法，同时 `CandidateList` 的主词排版切到 `textInputFontFamily`，更贴近真实输入法文字渲染路径
- 收紧了 demo 候选窗预览的体积：去掉底部分页提示区，把弹层宽度压到原来约一半，并通过减小候选项高度与内边距把整体高度进一步缩到更接近紧凑候选框的形态
- 继续修正了紧凑候选窗预览的内部排版：重新分配了序号/主词/注解三列宽度，避免主词区被挤到几乎不可见；同时把顶部拼音改为非粗体并大幅收窄上下留白
- 继续把 demo 候选窗预览往现有 HTML 候选框样式靠拢：非选中项现在不再绘制背景高亮或边框，仅保留选中项的深灰底和左侧细蓝条，同时进一步压缩了候选项之间的纵向间距
- 继续微调了候选窗预览里选中项的几何关系：左侧细蓝条进一步贴近外缘、宽度更细，序号与主词整体略向右挪，让高亮行里的文字位置更接近现有 HTML 候选框截图
- 继续收紧了候选窗预览的视觉比例：选中项高亮底块现在改成内收式胶囊区域，同时进一步下调数字、主词、注解三者的字号和灰度对比，让主词更接近现有 HTML 候选框的重心
- 继续微调了候选窗预览的外框和高亮细节：外框边线与阴影现在更轻、更暗，`ni` 与第一项之间的距离进一步收紧，选中项灰底和左侧蓝条的高度关系也更接近现有 HTML 候选框截图
- 继续按 HTML 参考图收紧候选窗预览：候选项从分栏感更强的布局进一步改成接近 inline 的节奏，整体宽度和行高再次缩小，选中项高亮块也改得更像原始候选框的窄条形态
- 继续把候选窗预览往 1:1 参考图上贴：蓝条现在按高亮块左边缘中心线定位，候选项文本整体进一步向“1你(rX)”这种紧凑串联节奏收拢，外框与行高也再次缩小
- 继续压缩了候选窗预览的横向节奏：整体宽度、圆角和左侧内边距再次收窄，数字/主词/注解之间的间距进一步缩小，更贴近原始 HTML 候选框那种紧凑的一行串联感
- 回调了一版候选窗预览的整体尺度：在保留当前紧凑结构关系的前提下，适度增大了外框宽度、行高以及 `ni` / 候选词 / 注解的字号，避免预览相对原始 HTML 候选框显得过小
- 为 `TextBlock` 补上了可选字体族覆盖能力，demo 候选窗预览中的 `ni` 与候选项文本现统一切到 `Noto Sans SC`；同时继续缩小了高亮块的外边距和内部留白，并把 `ni` 左对齐到第一排候选序号
- 继续微调了候选窗预览的字重和顶部位置：候选汉字从半粗体回调到更克制的常规字重，同时把顶部 `ni` 略微下移，使其更接近原始候选框截图里的垂直位置
- 在保留你手动调到 `16` 号候选字的基础上，继续放宽了选中项高亮矩形的宽度，并把 `ni` 与第一条候选项之间的垂直空白压到约 `2px`

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

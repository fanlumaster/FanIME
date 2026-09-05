# Metasequoia IME UI HTML

水杉输入法 Windows 端的界面资源。Server 加载这里的页面来渲染候选窗、悬浮工具栏、托盘菜单和设置窗口。

配套仓库：[MSIME-Windows](https://github.com/metasequoiaime/MSIME-Windows)（TSF 前端）、[MSIME-Server](https://github.com/metasequoiaime/MSIME-Server)（后端与窗口宿主）。

## 目录

| 路径 | 内容 |
|---|---|
| `webview2/candwnd/` | 候选窗，含 `skins/` 下的皮肤 |
| `webview2/ftb/` | 悬浮工具栏 |
| `webview2/menu/` | 托盘菜单 |
| `webview2/settings/ime-settings/` | 设置页，**唯一需要构建**的部分，Vite + TypeScript |
| `webview2/default-themes/`、`webview2/other-themes/` | 主题 |

除设置页外都是直接分发的静态资源，改完即生效。

## 构建设置页

设置页是发布安装包的必要输入之一，`pnpm build` 的产物 `dist/` 会被打包脚本收集。

```bash
cd webview2/settings/ime-settings
corepack enable
pnpm install --frozen-lockfile
pnpm build
```

pnpm 版本由 `package.json` 的 `packageManager` 字段固定，`corepack enable` 之后不用自己装。开发时用 `pnpm dev` 起本地服务。

## 本地调试

把本仓链接到 Server 的数据目录，改页面就不用重新打包：

```powershell
$target = Join-Path $env:LOCALAPPDATA 'metasequoiaime\html'
if (Test-Path -LiteralPath $target) {
    Remove-Item -LiteralPath $target -Recurse -Force
}
New-Item -ItemType SymbolicLink -Path $target -Target (Resolve-Path .).Path
```

必须用绝对路径，`Resolve-Path` 就是用来把当前目录转成绝对路径的。设置页走这条路时链接的仍是仓库目录，页面读的是 `webview2/settings/ime-settings/dist/`，所以改完设置页要重新 `pnpm build`。

## 与 C++ 侧的边界

窗口归 Server，页面归本仓。HWND、尺寸、位置、DPI、Z-order 和 WebView2 controller 的生命周期都在 `MSIME-Server/src/window/` 与 `src/webview2/`；页面结构、样式和浏览器端交互在这里。

C++ → 页面主要经 WebView2 导航和脚本执行，页面 → C++ 经 `window.chrome.webview.postMessage(...)`。**改消息 `type`、JSON 字段、页面导出的 JS 函数或 DOM 标识时，必须同步检查 Server 的消息解析与拼接代码**，两边没有编译期约束。

候选内容和输入状态的权威在 Server 和引擎，页面只负责展示与发出用户动作；不要在页面侧复制候选选择、翻页、输入模式或配置持久化的状态机。

完整的跨仓约定见 [MSIME-Windows 的 AGENTS.md](https://github.com/metasequoiaime/MSIME-Windows/blob/main/AGENTS.md)。

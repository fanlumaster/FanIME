# 安装水杉输入法

> 适用版本：v0.0.9.2（2026 年 8 月 25 日发布，内测版）
> 若你安装的是更新版本，请以 [Releases](https://github.com/metasequoiaime/MetasequoiaImeTsf/releases) 中的说明为准。

## 目录

- [环境要求](#环境要求)
- [下载](#下载)
- [安装与注意事项](#安装与注意事项)
- [在Windows中启用水杉](#在Windows中启用水杉)
- [常见问题](#常见问题)

## 环境要求

| 项目 | 要求 |
|---|---|
| 操作系统 | Windows 10 / Windows 11 |
| 处理器架构 | 同时提供 64 位与 32 位组件，x64 / x86 Windows 均可安装 |
| 磁盘空间 | 安装包约 61 MB，安装后占用约 250 MB［待确认：实测安装后目录大小］ |
| 网络 | 云联想、AI 联想、语音输入、候选词翻译等在线功能需要联网；离线状态下本地输入不受影响 |
| 其他 | 使用手写识别需先在 Windows 中安装「中文手写包」 |
| Visual C++ 运行库 | 无需单独安装：TSF DLL 采用静态 CRT 构建（Release 为 `/MT`），不依赖宿主的 MSVCP140 / VCRUNTIME140 |

## 下载

1. 打开 [Releases 页面](https://github.com/metasequoiaime/MetasequoiaImeTsf/releases)。
2. 下载最新的安装包 `MetasequoiaIME_Setup_v<版本号>.exe`（例如 `MetasequoiaIME_Setup_v0.0.9.2.exe`）。

> 注意：当前发布版本均标记为 **Pre-release（内测版）**，功能与稳定性可能变动，重要环境建议先在虚拟机或非主力机试用。
> 参与内测与反馈：[Telegram 群](https://t.me/msimegroup)、QQ 群 829919142。

## 安装与注意事项

双击安装包，按安装向导提示完成安装。

- 安装默认路径是C:\Program Files\MetasequoiaIme
- 安装程序会把水杉输入法加入输入法列表，无须手动添加
- 安装完成后自动启动 `MetasequoiaImeServer`

## 在Windows中启用水杉

使用 `Win + 空格` 在已安装的输入法之间切换，或用 `Ctrl + Shift` 切换。

## 常见问题


# 水杉输入法

一款开源的自由 Windows 输入法。

## 项目结构

- [MetasequoiaImeTsf](https://github.com/metasequoiaime/MetasequoiaImeTsf): 核心 TSF。
- [MetasequoiaImeServer](https://github.com/metasequoiaime/MetasequoiaImeServer): Server 端，负责算法和窗口渲染。
- [MetasequoiaImeUiHtml](https://github.com/metasequoiaime/MetasequoiaImeUiHtml): UI。
- [MetasequoiaImeDict](https://github.com/metasequoiaime/MetasequoiaImeDict): 词库。
- [MetasequoiaImeHelpCode](https://github.com/metasequoiaime/MetasequoiaImeHelpCode): 辅助码。

## 功能简介

- 支持全拼、双拼、五笔86。
- 拼音方案支持辅助码。
- 支持谷歌云联想。
- 支持 DeepSeek AI 联想。
- 支持词库管理、批量导入、导出。
- 支持手写识别。
- 支持快捷短语。

## 核心功能指南

### 辅助码

辅助码是一种可选的形码筛选功能。输入完整的拼音后，在末尾追加辅助码，可以缩小候选范围或调整候选顺序。

辅助码方案可以在配置中选择，目前支持：

- 蓝天小雨点
- 自然码
- 首右 2.0
- 首右 Plus
- 小鹤

辅助码有“单码”和“双码”两种使用方式，也可以分别用于全拼和双拼。

#### 单码辅助

在完整的拼音或双拼编码后追加一个辅助码。

单码辅助主要用于调整候选顺序：

- 单字：辅助码匹配汉字的任意一个形码。
- 词组：辅助码匹配词组首字或末字的首个形码。
- 匹配到的候选会被优先显示，但其他候选仍然保留。

例如，使用自然码辅助码时，“阿”的辅助码为 `ek`：

```text
全拼：aE
双拼：aae 或者 aaE
```

其中 E 表示追加的辅助码。实际输入时可以按住 Shift 输入大写字母。

注意：双拼追加的单码大小写都可以，但是全拼追加的单码必须是大写。

#### 双码辅助

在完整的拼音或双拼编码后追加两个辅助码。

**双码辅助**会严格筛选候选：

- 单字：两个辅助码分别匹配该字的两个形码。
- 词组：第一个辅助码匹配首字的首个形码，第二个辅助码匹配末字的首个形码。
- 筛选结果只保留符合辅助码的候选。

例如，“阿”的辅助码为 `ek`：

```text
全拼：aEK
双拼：aaeK
```

全拼输入时，两个辅助码使用大写字母；双拼输入时，通常使用“小写辅助码 + 大写辅助码”的形式，以便和单码辅助区分。

**全拼辅助**是在完整的全拼后追加辅助码：

```text
阿：aE       # 单码辅助
阿：aEK      # 双码辅助
阿姨：ayiE   # 单码辅助
阿姨：ayiEN  # 双码辅助
```

**双拼辅助**是在完整的双拼编码后追加辅助码。以小鹤双拼为例：

```text
阿：aae       # 单码辅助
阿：aaE       # 单码辅助
阿：aaeK      # 双码辅助
阿：aaEK      # 双码辅助
阿姨：aayie   # 单码辅助
阿姨：aayiE   # 单码辅助
阿姨：aayieN  # 双码辅助
阿姨：aayiEN  # 双码辅助
```

**注意**：辅助码只有在前面的全拼或双拼编码完整时才会生效。如果前面的拼音尚未完整，末尾输入的字母仍会按照普通拼音处理。

### 云联想

当前使用的谷歌的云联想 API，对网络有所要求。云联想如果返回了当前本地不存在的结果，会插入在首页的第二项。

### AI 联想

当前 AI 联想使用了 DeepSeek 的 API。AI 联想如果返回了本地以及 云联想都不存在的结果，会插入在首页的第三项。

### 候选项调频

可以在设置中自行选定调频算法。

### preedit 显示设置

用户可以通过设置->外观->行内预编辑和设置->外观->候选窗预编辑来控制应用内的拼音串和候选框内的拼音串的显示风格以及是否显示。

## 软件截图

<img src="https://i.imgur.com/1gDQFix.png" width="750">

<img src="https://i.imgur.com/MxRcKYT.png" width="750">

<img src="https://i.imgur.com/5MG33lH.png" width="750">

<img src="https://i.imgur.com/gnJwoVa.png" width="750">

<img src="https://i.imgur.com/wBwpuUT.png" width="750">

<img src="https://i.imgur.com/VcjIaeV.png" width="750">

<!-- 暗色 -->

<img src="https://i.imgur.com/YgxjjEz.png" width="780">

<!-- 亮色 -->

<img src="https://i.imgur.com/dkXHbPm.png" width="780">

## 开源协议

GPL-3.0。

## 参考

以下是一些 Windows 的开源输入法的参考项目。

- [Microsoft TSF IME Demo](https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/IME/cpp/SampleIME)
- [weasel](https://github.com/rime/weasel)
- [PIME](https://github.com/EasyIME/PIME)
- [WindInput](https://github.com/huanfeng/WindInput)

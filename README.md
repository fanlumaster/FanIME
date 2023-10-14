## What is FanIME

以 Microsft 官方给出的 [TSF IME Demo](https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/IME/cpp/SampleIME) 为基础开发的一款自定义的 Windows11 平台下的输入法。

原 Demo 是基于 Windows8 进行构建，所以，本项目的工作首先是解决兼容性问题。更多的功能问题，详见下面的 Roadmap。

## How to install

```
regsvr32 "C:\Program Files\SampleIME\SampleIME.dll"
regsvr32 "C:\Program Files (x86)\SampleIME\SampleIME.dll"
```

how to uninstall ime

```shell
regsvr32 /u "C:\Program Files\SampleIME\SampleIME.dll"
regsvr32 /u "C:\Program Files (x86)\SampleIME\SampleIME.dll"
```

## Roadmap

### 中文

- 全拼
- 小鹤双拼
- 辅助码
- Direct2D 绘制输入法异形窗口
- DWrite 渲染候选框字体
- 词库定制
- 词库引擎定制(候选项调整、联想组词)
- 皮肤自定义
- 五笔
- 简/繁切换
- 英文自动补全
- 云输入接口自开发，数据和接口开源公开
- 输入法候选框横向/纵向列表切换
- 功能开关：大部分功能都应以开关的形式提供选项，让用户自由选择开启或关闭


### 日文

### Other languages



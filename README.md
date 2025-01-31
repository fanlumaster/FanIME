## What is FanIME

Based on Microsoft [TSF IME Demo](https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/IME/cpp/SampleIME) 。

Mainly to develop a Chinese Input Method Editor without ads and likely some other disturbing things we users hate really.

The roadmap is as follows.

## Build and Install and Uninstall

### How to build

Run the following command,

```powershell
./lcompile.ps1
```

### How to Install

Run the following command as administrator,

```powershell
./linstall.ps1
```

### How to Uninstall

```shell
./luninstall.ps1
```

## Roadmap

### Chinese

- Quanpin
- Xiaohe Shuangpin
- Help codes in use of Hanzi Components
- Direct2D to draw candidate windows
- DirectWrite to render candidate windows fonts
- Ciku that can be customized
- Customized IME engine
- Customized skins
- Toggle between Simplified Chinese and Traditional Chinese
- English autocomplete
- Open-Soured Cloud IME api
- Toggle candidate window UI between vertical mode and horizontal mode
- Feature switches: most features should be freely toggled or customized by users

### Japanese Support

Maybe another project.

### Other languages

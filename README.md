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

we also need to build one 32-bit version,

```powershell
./lcompile.ps1 32
```

Besides, we also need to build dictionary for our IME using this repositoy below,

<https://github.com/fanlumaster/FanyDictForIME.git>

You need to build this this repo to generate IME Dictionary first, what you need to do is running the commands besides using pwsh7,

```powershell
cd $env:LOCALAPPDATA
mkdir DeerWritingBrush
cd DeerWritingBrush
git clone https://github.com/fanlumaster/FanyDictForIME.git
cd ./FanyDictForIME/makecikudb/xnheulpb/makedb/separated_jp_version
python ./create_db_and_table.py
python ./insert_data.py
python ./create_index_for_db.py
Copy-Item -Path ./out/cutted_flyciku_with_jp.db -Destination $env:LOCALAPPDATA/DeerWritingBrush
```

Moreover, we also need to copy another Dictionary file contained in our main project repo(FanIME) to `$env:LOCALAPPDATA/DeerWritingBrush`，run the command below,

```powersehll
Copy-Item -Path ./Dictionary/SampleIMESimplifiedQuanPin.txt -Destination $env:LOCALAPPDATA/DeerWritingBrush
```

Last, copy following files to `$env:LOCALAPPDATA/DeerWritingBrush`,

```powersehll
Copy-Item -Path ./assets/* -Destination $env:LOCALAPPDATA/DeerWritingBrush
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

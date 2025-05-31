# FanImeServer

This is the server end of [FanIMETsf](https://github.com/fanlumaster/FanImeTsf).

## How to build

### Prerequisites

- Visual Studio 2022
- CMake
- vcpkg
- Nuget
- Python3

Make sure vcpkg and Nuget are installed by **Scoop**.

### Build steps

**First**, build IME dictonary and prepare assets,

```powershell
cd $env:LOCALAPPDATA
mkdir FanImeTsf
cd FanImeTsf
git clone https://github.com/fanlumaster/FanyDictForIME.git
cd .\FanyDictForIME\makecikudb\xnheulpb\makedb\separated_jp_version
python .\create_db_and_table.py
python .\insert_data.py
python .\create_index_for_db.py
Copy-Item -Path .\out\cutted_flyciku_with_jp.db -Destination $env:LOCALAPPDATA\FanImeTsf
```

**Then**, clone and build FanImeServer,

```powershell
git clone --recursive https://github.com/fanlumaster/FanImeServer.git
```

Prepare environment,

```powershell
cd FanImeServer
python .\scripts\prepare_env.py
Copy-Item -Path .\assets\tables\* -Destination $env:LOCALAPPDATA\FanImeTsf
```

Then, build and run,

```powershell
.\scripts\lcompile.ps1
.\scripts\lrun.ps1
```

If you want to build and run in **one step**, run the following command,

```powershell
.\scripts\llaunch.ps1
```

## Screenshots

![](https://i.postimg.cc/v8Bpx6Gf/image.png)

![](https://i.postimg.cc/ssBgtM5M/image.png)

![](https://i.postimg.cc/ryDqXH0B/image.png)

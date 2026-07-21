# Metasequoia IME Server(水杉输入法 Server 端)

This is the server end of [MetasequoiaImeTsf](https://github.com/metasequoiaime/MetasequoiaImeTsf.git).

## How to build

### Prerequisites

- Visual Studio 2026
- CMake
- vcpkg
- Python3.10+
- Boost

Make sure vcpkg and Boost are installed by **Scoop**.

### Build steps(For Dev)

**First**, build IME dictonary and prepare assets,

```powershell
cd $env:LOCALAPPDATA
mkdir metasequoiaime
cd metasequoiaime
git clone --recursive https://github.com/metasequoiaime/MetasequoiaImeDict.git
cd .\MetasequoiaImeDict\makecikudb\xnheulpb\makedb\separated_jp_version
python .\create_db_and_table.py
python .\insert_data.py
python .\create_index_for_db.py
Copy-Item -Path .\out\msime.db -Destination $env:LOCALAPPDATA\metasequoiaime
```

**Then**, clone and build MetasequoiaImeServer,

```powershell
git clone --recursive https://github.com/metasequoiaime/MetasequoiaImeServer.git
```

Prepare environment,

```powershell
cd MetasequoiaImeServer
python .\scripts\prepare_env.py
Copy-Item -Path .\assets\tables\* -Destination $env:LOCALAPPDATA\metasequoiaime
New-Item -ItemType SymbolicLink -Path "$env:LOCALAPPDATA\metasequoiaime\config.toml" -Target ".\assets\config\config.toml"
```

e.g.

```powershell
cd MetasequoiaImeServer
python .\scripts\prepare_env.py
Copy-Item -Path .\assets\tables\* -Destination $env:LOCALAPPDATA\metasequoiaime
New-Item -ItemType SymbolicLink -Path "C:\Users\sonnycalcr\AppData\Local\metasequoiaime\config.toml" -Target "C:\Users\sonnycalcr\EDisk\CppCodes\IMECodes\MetasequoiaImeServer\assets\config\config.toml"
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

## Watchdog

`MetasequoiaImeWatchdog.exe` is built next to the server. Starting the server directly also starts the watchdog,
which monitors the server from the same directory and restarts it after an unexpected exit. Repeated early crashes use
an exponential restart delay (up to 30 seconds) to avoid a restart storm.

The developer terminate shortcut stops both processes. The restart shortcut asks the watchdog to start a fresh server
after the current process exits.

## English prefix candidates

Place `english.db` next to `msime.db` in `%LOCALAPPDATA%\metasequoiaime`. Set
`general.cn_en_mixed_input = true` in `config.toml` to enable asynchronous English prefix candidates for Quanpin and
Shuangpin.

## Developer Shortcuts

The server process includes a few built-in global shortcuts that are useful during development:

- `Ctrl + Shift + Alt + T`
  - terminate `MetasequoiaImeServer` immediately
- `Ctrl + Shift + Alt + R`
  - restart `MetasequoiaImeServer`
- `Ctrl + Shift + Alt + C`
  - clear IME engine cache
- `Ctrl + Shift + Alt + 1` to `Ctrl + Shift + Alt + 8`
  - delete candidate `1` to `8` from the current candidate window
  - this only works while the candidate window is visible

## Screenshots

![](https://i.postimg.cc/c402J3KR/image.png)

![](https://i.postimg.cc/v8Bpx6Gf/image.png)

![](https://i.postimg.cc/ssBgtM5M/image.png)

![](https://i.postimg.cc/ryDqXH0B/image.png)

![](https://i.postimg.cc/2m9WJTgR/image.png)

![](https://i.postimg.cc/L96qQZT8/image.png)

![](https://i.postimg.cc/FNcz9QTv/image.png)

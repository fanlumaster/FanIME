# Server end of FanImeTsf

This is the server end of [FanIMETsf](https://github.com/fanlumaster/FanImeTsf).

## How to build

### Prerequisites

- Visual Studio 2022
- CMake
- vcpkg
- Nuget
- Python3

Make sure vcpkg and Nuget are installed by **scoop**.

### Build steps

```powershell
git clone --recursive https://github.com/fanlumaster/FanImeServer.git
cd FanImeServer
python .\scripts\prepare_env.py
.\scripts\lcompile.ps1
```

Then, run the executable file that has already been compiled before,

```powershell
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

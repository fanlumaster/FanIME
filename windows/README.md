# windows —— TSF 文本服务

水杉输入法在 Windows 上的前端：一个纯 TSF 方案的文本服务 DLL，注入宿主进程，负责按键预判、焦点与线程归属、composition 的 edit session 写入，以及与 Server 之间的命名管道协议。候选窗、工具栏、设置页和实际的候选计算都在 [`../server/`](../server/)，本目录只做宿主适配。

产品介绍、安装和功能说明见仓库根目录的 [README](../README.md)；本目录的代码地图、TSF/COM 硬约定和改动要求见 [AGENTS.md](AGENTS.md)。

## 构建

从仓库根目录配置，源码目录是本目录：

```powershell
cmake -S windows -B windows/build -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build windows/build --config Release --parallel
ctest --test-dir windows/build -C Release --output-on-failure
```

Win32 用 `-A Win32` 和 `x86-windows-static`；安装包两种架构都要。`scripts/` 下是本机开发用的生成、构建、注册和签名脚本，从本目录运行。

引擎契约来自 `../vendor/MetasequoiaImeEngine`，需要 `git submodule update --init --recursive`。

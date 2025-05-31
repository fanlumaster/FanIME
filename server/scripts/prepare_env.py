import os


def normpath(path):
    return path.replace("\\", "/")


local_app_data_dir = os.environ.get("LOCALAPPDATA")
home_dir = os.path.expanduser("~")

cur_file_path = os.path.dirname(os.path.abspath(__file__))
project_root_path = os.path.dirname(cur_file_path)

user_home = os.path.expanduser("~")

FanImeServer_root_path = normpath(project_root_path)
FanImeServer_src_path = normpath(os.path.join(FanImeServer_root_path, "src"))
vcpkg_include_path = normpath(
    os.path.join(
        FanImeServer_root_path,
        "build",
        "vcpkg_installed",
        "x64-windows",
        "include",
    )
)
utfcpp_path = normpath(
    os.path.join(FanImeServer_root_path, "FanImeEngine", "utfcpp", "source")
)
webview2_path = normpath(
    os.path.join(
        user_home,
        ".nuget",
        "packages",
        "microsoft.web.webview2",
        "1.0.3240.44",
        "build",
        "native",
        "include",
    )
)
wim_path = normpath(
    os.path.join(
        user_home,
        ".nuget",
        "packages",
        "microsoft.windows.implementationlibrary",
        "1.0.240803.1",
        "include",
    )
)
boost_path = normpath(os.path.join(user_home, "scoop", "apps", "boost", "current"))

#
# project_root/.clangd
#
dot_clangd_file = os.path.join(
    FanImeServer_root_path, "scripts", "config_files", ".clangd"
)
dot_clangd_output_file = os.path.join(FanImeServer_root_path, ".clangd")
with open(dot_clangd_file, "r", encoding="utf-8") as f:
    lines = f.readlines()
lines[6] = f'      "-I{FanImeServer_root_path}",\n'
lines[7] = f'      "-I{FanImeServer_src_path}",\n'
lines[8] = f'      "-I{vcpkg_include_path}",\n'
lines[10] = f'      "-I{utfcpp_path}",\n'
lines[12] = f'      "-I{webview2_path}",\n'
lines[13] = f'      "-I{wim_path}",\n'
lines[15] = f'      "-I{boost_path}",\n'
with open(dot_clangd_output_file, "w", encoding="utf-8") as f:
    f.writelines(lines)

#
# project_root/tests/CMakeLists.txt
#
CMakeLists_file = os.path.join(
    FanImeServer_root_path, "scripts", "config_files", "CMakeLists.txt"
)
CMakeLists_output_file = os.path.join(FanImeServer_root_path, "CMakeLists.txt")
with open(CMakeLists_file, "r", encoding="utf-8") as f:
    lines = f.readlines()
lines[7] = f'set(Boost_ROOT "{boost_path}")\n'
with open(CMakeLists_output_file, "w", encoding="utf-8") as f:
    f.writelines(lines)

#
# CMakePresets.json
#
CMakePresets_file = os.path.join(
    FanImeServer_root_path, "scripts", "config_files", "CMakePresets.json"
)
CMakePresets_file_output_file = os.path.join(
    FanImeServer_root_path, "CMakePresets.json"
)
with open(CMakePresets_file, "r", encoding="utf-8") as f:
    lines = f.readlines()
lines[8] = f'        "VCPKG_ROOT": "{normpath(user_home)}/scoop/apps/vcpkg/current/"\n'
lines[11] = (
    f'        "CMAKE_TOOLCHAIN_FILE": "{normpath(user_home)}/scoop/apps/vcpkg/current/scripts/buildsystems/vcpkg.cmake",\n'
)
with open(CMakePresets_file_output_file, "w", encoding="utf-8") as f:
    f.writelines(lines)

#
# project_root/tests/CMakeLists.txt
#
Tests_CMakeLists_file = os.path.join(
    FanImeServer_root_path, "scripts", "config_files", "tests", "CMakeLists.txt"
)
Tests_CMakeLists_output_file = os.path.join(
    FanImeServer_root_path, "tests", "CMakeLists.txt"
)
with open(Tests_CMakeLists_file, "r", encoding="utf-8") as f:
    lines = f.readlines()
lines[10] = f'set(Boost_ROOT "{boost_path}")\n'
with open(Tests_CMakeLists_output_file, "w", encoding="utf-8") as f:
    f.writelines(lines)

#
# project_root/tests/CMakePresets.json
#
Tests_CMakePresets_file = os.path.join(
    FanImeServer_root_path, "scripts", "config_files", "tests", "CMakePresets.json"
)
Tests_CMakePresets_output_file = os.path.join(
    FanImeServer_root_path, "tests", "CMakePresets.json"
)
with open(Tests_CMakePresets_file, "r", encoding="utf-8") as f:
    lines = f.readlines()
lines[8] = f'        "VCPKG_ROOT": "{normpath(user_home)}/scoop/apps/vcpkg/current/"\n'
lines[11] = (
    f'        "CMAKE_TOOLCHAIN_FILE": "{normpath(user_home)}/scoop/apps/vcpkg/current/scripts/buildsystems/vcpkg.cmake",\n'
)
with open(Tests_CMakePresets_output_file, "w", encoding="utf-8") as f:
    f.writelines(lines)

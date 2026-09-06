#!/usr/bin/env bash
set -euo pipefail

# Fail if a build input names somebody's home directory.
#
# CI never noticed these because it does not use the presets: ci.yml and release.yml pass
# -DCMAKE_TOOLCHAIN_FILE explicitly, and release.yml even carries a comment saying the presets point
# at a developer machine and cannot be used. So the paths only broke people following the README,
# which is exactly the population this repository is trying to recruit.
#
# Editor configuration is deliberately out of scope: the .clangd files are generated per machine by
# each component's scripts/prepare_env.py and are expected to hold absolute local paths.

cd "$(git rev-parse --show-toplevel)"

pattern='(C:[\\/]+[Uu]sers[\\/]+|/Users/|/home/)[A-Za-z0-9_.-]+'

# Listed into a file rather than an array: mapfile is bash 4+, and macOS still ships bash 3.2, so a
# contributor running this locally would otherwise get a confusing "command not found".
candidates=$(git ls-files \
    '*.cmake' \
    'CMakeLists.txt' \
    '**/CMakeLists.txt' \
    '*CMakePresets.json' \
    '*CMakeUserPresets.json' \
    '*.md' \
    | grep -v '^vendor/' || true)

if [[ -z "$candidates" ]]; then
    echo 'No build inputs to check.'
    exit 0
fi

if matches=$(printf '%s\n' "$candidates" | tr '\n' '\0' | xargs -0 grep -InE "$pattern"); then
    echo 'Build inputs must not hardcode a developer home directory.'
    echo 'Use $env{VCPKG_ROOT} in presets, an environment variable with a documented fallback in CMake, and a relative link in Markdown.'
    echo
    echo "$matches"
    exit 1
fi

echo 'No developer-specific paths in build inputs.'

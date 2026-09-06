#!/usr/bin/env bash
set -euo pipefail

# Formatting has to be reproducible across contributor machines and CI, so the
# formatter version is pinned here and installed from PyPI instead of being
# taken from whatever the host toolchain happens to ship. Apple, Debian and
# LLVM upstream all disagree about clang-format defaults between releases.
#
# Ported from MSIME-Linux, which has run this in CI since before the
# consolidation. Each component keeps its own .clang-format; --style=file picks
# up the nearest one, so server/, windows/, ui/ and log/ stay independent.
clang_format_version=18.1.8

project_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
venv_root=${METASEQUOIA_CLANG_FORMAT_VENV:-"${TMPDIR:-/tmp}/metasequoia-clang-format-$clang_format_version"}
clang_format="$venv_root/bin/clang-format"

if [[ ! -x "$clang_format" ]]; then
    python3 -m venv "$venv_root"
    "$venv_root/bin/pip" install --quiet "clang-format==$clang_format_version"
fi

mode=${1:---write}
case "$mode" in
    --check) clang_format_arguments=(--dry-run --Werror) ;;
    --write) clang_format_arguments=(-i) ;;
    *)
        echo "Usage: ${BASH_SOURCE[0]} [--check|--write]" >&2
        exit 2
        ;;
esac

cd "$project_root"
# --others --exclude-standard so a newly written file that has not been staged
# yet is still formatted. Without it the script reports a clean tree while
# skipping exactly the file being worked on.
#
# vendor/ and experiments/ are excluded: the first is third-party, the second is
# a scratch project that is not part of the shipped product.
git ls-files --cached --others --exclude-standard \
        'server/*.cpp' 'server/*.h' \
        'windows/*.cpp' 'windows/*.h' \
        'ui/*.cpp' 'ui/*.h' \
        'log/*.cpp' 'log/*.h' \
    | grep -v '/vendor/' \
    | sort -u \
    | xargs "$clang_format" "${clang_format_arguments[@]}" --style=file
